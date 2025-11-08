#include "llm_query.h"
#include <memory>
#include <string>
#include <utility>
#include "http_client.h"
#include "json.h"
#include "log.h"
#include "unittest.h"

using std::make_unique;
using std::string;
using std::unique_ptr;

namespace cbl {

class LLMQueryTest : public cbl::Test {
private:
  string computeLanguage(LLMClient& client, const string& text) {
    json::Value generationConfig = json::parse(R"({
      "responseMimeType": "application/json",
      "responseSchema": {
        "type": "OBJECT",
        "properties": {
          "code": { "type": "STRING" },
          "name": { "type": "STRING" }
        },
        "propertyOrdering": ["code", "name"]
      }
    })");
    LLMResponse response = client.generateResponse(
        {.text = "Compute the language of the following text, giving the ISO 639-1 language code as \"code\" and the "
                 "name of the language in English as \"name\".\nInput text:\n" +
                 text,
         .generationConfig = generationConfig.copy()});
    json::Value parsedResponse = json::parse(response.text);
    return parsedResponse["code"].str() + "," + parsedResponse["name"].str();
  }

  CBL_TEST_CASE(LLMClient) {
    unique_ptr<HTTPClientWithCache> httpClient = make_unique<HTTPClientWithCache>();
    httpClient->setCacheDir("testdata/llm_cache");
    httpClient->setCacheMode(HTTPClientWithCache::CACHE_ENABLED | HTTPClientWithCache::CACHE_POST);

    LLMClient client(std::move(httpClient));
    client.setDelayBetweenQueries({});
    CBL_ASSERT_EQ(computeLanguage(client, "Je suis un humain, pas une machine !"), "fr,French");
    CBL_ASSERT_EQ(computeLanguage(client, "Which LLM is the best?"), "en,English");
    CBL_ASSERT_EQ(computeLanguage(client, "Das Telefon klingelt!"), "de,German");
  }

  CBL_TEST_CASE(LLMClientWithCache) {
    LLMClientWithCache client("testdata/llm_cache/llm_client_with_cache.json");
    CBL_ASSERT_EQ(computeLanguage(client, "Je suis un humain, pas une machine !"), "fr,French");
    CBL_ASSERT_EQ(computeLanguage(client, "Which LLM is the best?"), "en,English");
    CBL_ASSERT_EQ(computeLanguage(client, "Das Telefon klingelt!"), "de,German");
    client.saveCachedResponses(false);
  }
};

}  // namespace cbl

int main() {
  cbl::LLMQueryTest().run();
  return 0;
}
