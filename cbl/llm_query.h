#ifndef CBL_LLM_QUERY_H
#define CBL_LLM_QUERY_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "date.h"
#include "http_client.h"
#include "json.h"

namespace cbl {

struct LLMQuery {
  std::string text;
  int thinkingBudget = -1;
  bool includeThoughts = false;
  json::Value generationConfig;
};

struct LLMResponse {
  std::string text;
  std::string thought;
};

class LLMClient {
public:
  explicit LLMClient(std::unique_ptr<HTTPClient> httpClient = nullptr);
  ~LLMClient() = default;
  virtual LLMResponse generateResponse(const LLMQuery& query);
  void setDelayBetweenQueries(cbl::DateDiff delay) { m_delayBetweenQueries = delay; }

private:
  std::unique_ptr<HTTPClient> m_httpClient;
  cbl::Date m_lastQueryDate;
  cbl::DateDiff m_delayBetweenQueries = cbl::DateDiff::fromSeconds(12);
};

}  // namespace cbl

#endif
