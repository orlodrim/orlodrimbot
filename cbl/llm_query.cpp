#include "llm_query.h"
#include <unistd.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "date.h"
#include "file.h"
#include "http_client.h"
#include "json.h"
#include "log.h"
#include "sha1.h"
#include "string.h"

using std::make_unique;
using std::pair;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;

namespace cbl {

string postWithRetries(HTTPClient& client, const string& url, const string& data, int maxAttempts = 3) {
  for (int attemptsLeft = maxAttempts;; attemptsLeft--) {
    try {
      return client.post(url, data);
    } catch (const cbl::HTTPServerError& error) {
      if (attemptsLeft <= 0 || error.httpCode() != 503) {
        throw;
      }
      CBL_WARNING << "LLM query failed: " << error.what() << " (" << attemptsLeft - 1 << " attempts left)";
      sleep(5);
    }
  }
}

LLMClient::LLMClient(unique_ptr<HTTPClient> httpClient) : m_httpClient(std::move(httpClient)) {
  if (!m_httpClient) {
    m_httpClient = make_unique<HTTPClient>();
  }
  const char* apiKey = getenv("GEMINI_API_KEY");
  if (apiKey != nullptr && *apiKey) {
    m_httpClient->addHeader(cbl::concat("x-goog-api-key: ", apiKey));
  }
  m_httpClient->addHeader("Content-Type: application/json");
}

LLMResponse LLMClient::generateResponse(const LLMQuery& query) {
  if (m_delayBetweenQueries.seconds() != 0 && !m_lastQueryDate.isNull()) {
    int secondsToWait = (m_delayBetweenQueries - (Date::now() - m_lastQueryDate)).seconds();
    if (secondsToWait > 0) {
      sleep(secondsToWait);
    }
  }
  m_lastQueryDate = Date::now();

  const string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
  json::Value queryObj;
  queryObj.getMutable("contents").getMutable("parts").getMutable("text") = query.text;
  if (!query.generationConfig.isNull()) {
    queryObj.getMutable("generationConfig") = query.generationConfig.copy();
  }
  if (query.thinkingBudget != -1) {
    json::Value& thinkingConfig = queryObj.getMutable("generationConfig").getMutable("thinkingConfig");
    thinkingConfig.getMutable("thinkingBudget") = query.thinkingBudget;
    if (query.includeThoughts) {
      thinkingConfig.getMutable("includeThoughts") = true;
    }
  }
  string rawResponse = postWithRetries(*m_httpClient, url, queryObj.toJSON());
  // CBL_INFO << "rawResponse=" << rawResponse;
  json::Value responseObj = json::parse(rawResponse);
  const json::Value& parts = responseObj["candidates"][0]["content"]["parts"];
  LLMResponse response;
  for (const json::Value& part : parts.array()) {
    if (part["thought"].boolean()) {
      if (response.thought.empty()) {
        response.thought = part["text"].str();
      }
    } else if (response.text.empty()) {
      response.text = part["text"].str();
    }
  }
  return response;
}

LLMClientWithCache::LLMClientWithCache(string_view cacheFile, std::unique_ptr<HTTPClient> httpClient)
    : LLMClient(std::move(httpClient)), m_cacheFile(cacheFile) {
  if (cbl::fileExists(m_cacheFile)) {
    string cacheText = cbl::readFile(m_cacheFile);
    m_cacheContentHash = cbl::sha1(cacheText);
    json::Value cacheObj = json::parse(cacheText);
    for (const auto& [queryKey, responseObj] : cacheObj.object()) {
      LLMResponse& response = m_cache[queryKey].first;
      response.text = responseObj["text"].str();
      response.thought = responseObj["thought"].str();
    }
  }
}

LLMResponse LLMClientWithCache::generateResponse(const LLMQuery& query) {
  string queryKey = getQueryKey(query);
  // If the response is not cached yet, we must not create an entry in the cache until it is successfully generated.
  unordered_map<string, pair<LLMResponse, bool>>::iterator responseIt = m_cache.find(queryKey);
  if (responseIt != m_cache.end()) {
    responseIt->second.second = true;
  } else {
    responseIt = m_cache.insert({std::move(queryKey), {LLMClient::generateResponse(query), true}}).first;
  }
  return responseIt->second.first;
}

void LLMClientWithCache::saveCachedResponses(bool keepUnused) {
  json::Value cacheObj;
  for (const auto& [queryKey, responseAndUsed] : m_cache) {
    if (!keepUnused && !responseAndUsed.second) {
      continue;
    }
    const LLMResponse& response = responseAndUsed.first;
    json::Value& responseObj = cacheObj.getMutable(queryKey);
    responseObj.getMutable("text") = response.text;
    if (!response.thought.empty()) {
      responseObj.getMutable("thought") = response.thought;
    }
  }
  string newContent = cacheObj.toJSON(json::INDENTED) + "\n";
  if (cbl::sha1(newContent) != m_cacheContentHash) {
    cbl::writeFile(m_cacheFile, newContent);
  }
}

std::string LLMClientWithCache::getQueryKey(const LLMQuery& query) {
  return cbl::sha1(cbl::concat(query.text, "|", std::to_string(query.thinkingBudget), "|",
                               query.includeThoughts ? "1" : "0", "|", query.generationConfig.toJSON()));
}

}  // namespace cbl
