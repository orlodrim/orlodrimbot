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

}  // namespace cbl
