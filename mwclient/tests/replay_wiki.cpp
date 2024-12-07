#include "replay_wiki.h"
#include <re2/re2.h>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
#include "cbl/file.h"
#include "cbl/http_client.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "mwclient/util/init_wiki.h"

using cbl::HTTPClient;
using std::string;
using std::unique_ptr;
using std::unordered_set;

namespace mwc {
namespace {

constexpr const char* ENABLE_RECORDING_VARIABLE = "MWCLIENT_TESTS_RECORDING";
constexpr int REPLAY_WIKI_API_LIMIT = 3;
constexpr int REPLAY_WIKI_API_TITLES_LIMIT = 2;

bool enableRecording() {
  const char* variable = getenv(ENABLE_RECORDING_VARIABLE);
  return variable && variable[0];
}

class RecordHTTPClient : public cbl::HTTPClient {
public:
  virtual void startTestCase(const string& name) = 0;
  virtual void endTestCase() = 0;
};

class RecordWriterHTTPClient : public RecordHTTPClient {
public:
  explicit RecordWriterHTTPClient(const string& outputFile) : m_outputStream(outputFile.c_str()) {
    m_outputStream << "{";
  }
  ~RecordWriterHTTPClient() { m_outputStream << "\n}\n"; }
  void startTestCase(const string& name) override {
    CBL_ASSERT(!m_inTestCase) << name;
    CBL_ASSERT(m_previousTestCases.count(name) == 0) << name;
    m_inTestCase = true;
    m_previousTestCases.insert(name);
    m_outputStream << (m_numTestCases > 0 ? ",\n" : "\n") << "  " << json::Value(name).toJSON() << ": [";
    m_numTestCases++;
    m_numRequestsDone = 0;
    CBL_INFO << "Starting recording for test case '" << name << "'";
  }
  void endTestCase() override {
    CBL_ASSERT(m_inTestCase);
    m_inTestCase = false;
    m_outputStream << "\n  ]";
  }
  string get(const string& url) override {
    CBL_ASSERT(m_inTestCase);
    string response = stripServedBy(HTTPClient::get(url));
    if (!isLoginError(response) && url.find("&action=query&meta=tokens&type=login") == string::npos) {
      startNewRequest();
      m_outputStream << "    {\n"
                     << "      \"method\": \"GET\",\n"
                     << "      \"url\": " << urlToJson(url, 6) << ",\n"
                     << "      \"response\": " << json::parse(response).toJSON() << "\n"
                     << "    }";
    }
    return response;
  }
  string post(const string& url, const string& data) override {
    CBL_ASSERT(m_inTestCase);
    string response = stripServedBy(HTTPClient::post(url, data));
    if (!isLoginError(response) && !data.starts_with("action=login&")) {
      startNewRequest();
      m_outputStream << "    {\n"
                     << "      \"method\": \"POST\",\n"
                     << "      \"url\": " << urlToJson(url, 6) << ",\n"
                     << "      \"data\": " << urlToJson(data, 6) << ",\n"
                     << "      \"response\": " << json::parse(response).toJSON() << "\n"
                     << "    }";
    }
    return response;
  }

private:
  static bool isLoginError(const string& response) {
    return response.find(R"("code":"assertuserfailed")") != string::npos;
  }
  static string stripServedBy(string response) {
    static const re2::RE2 reServedBy(R"("servedby":"mw\d+")");
    RE2::Replace(&response, reServedBy, R"("servedby":"mwXXXX")");
    return response;
  }
  // Splits an URL by query parameter (without too much parsing), so that diffs are easier to review.
  static string urlToJson(const string& url, int indentation) {
    string result = "[";
    size_t position = 0;
    for (int segmentIndex = 0; position < url.size(); segmentIndex++) {
      size_t nextDelimiter = url.find_first_of("?&", position);
      size_t endOfSegment = nextDelimiter == string::npos ? url.size() : nextDelimiter + 1;
      result += segmentIndex == 0 ? "\n" : ",\n";
      result += string(indentation + 2, ' ');
      result += json::Value(url.substr(position, endOfSegment - position)).toJSON();
      position = endOfSegment;
    }
    result += '\n';
    result += string(indentation, ' ');
    result += "]";
    return result;
  }
  void startNewRequest() {
    m_outputStream << (m_numRequestsDone > 0 ? ",\n" : "\n");
    m_numRequestsDone++;
  }

  std::ofstream m_outputStream;
  unordered_set<string> m_previousTestCases;
  bool m_inTestCase = false;
  int m_numTestCases = 0;
  int m_numRequestsDone = 0;
};

class RecordReaderHTTPClient : public RecordHTTPClient {
public:
  explicit RecordReaderHTTPClient(const string& inputFile) : m_testCases(json::parse(cbl::readFile(inputFile))) {}
  void startTestCase(const string& name) override {
    CBL_ASSERT(m_requests == nullptr) << name;
    m_currentTestCase = name;
    m_requests = &m_testCases[name];
    CBL_ASSERT(m_requests->isArray()) << name;
    m_numRequestsDone = 0;
  }
  void endTestCase() override {
    CBL_ASSERT(m_requests != nullptr);
    CBL_ASSERT_EQ(m_numRequestsDone, m_requests->array().size());
    m_currentTestCase.clear();
    m_requests = nullptr;
  }
  string get(const string& url) override {
    CBL_ASSERT(m_requests != nullptr);
    const json::Value& request = (*m_requests)[m_numRequestsDone];
    m_numRequestsDone++;
    CBL_ASSERT_EQ(request["method"].str(), "GET") << m_currentTestCase;
    CBL_ASSERT_EQ(urlFromJson(request["url"]), url) << m_currentTestCase;
    return request["response"].toJSON();
  }
  string post(const string& url, const string& data) override {
    CBL_ASSERT(m_requests != nullptr);
    const json::Value& request = (*m_requests)[m_numRequestsDone];
    m_numRequestsDone++;
    CBL_ASSERT_EQ(request["method"].str(), "POST") << m_currentTestCase;
    CBL_ASSERT_EQ(urlFromJson(request["url"]), url) << m_currentTestCase;
    CBL_ASSERT_EQ(urlFromJson(request["data"]), data) << m_currentTestCase;
    return request["response"].toJSON();
  }

private:
  static string urlFromJson(const json::Value& value) {
    CBL_ASSERT(value.isArray());
    string url;
    for (const json::Value& urlPart : value.array()) {
      url += urlPart.str();
    }
    return url;
  }

  json::Value m_testCases;
  string m_currentTestCase;
  const json::Value* m_requests = nullptr;
  int m_numRequestsDone = 0;
};

}  // namespace

ReplayWiki::ReplayWiki(const string& testName, AccountType accountType) {
  string dataPath = "testdata/replay/" + testName + ".json";
  if (enableRecording()) {
    setHTTPClient(unique_ptr<cbl::HTTPClient>(new RecordWriterHTTPClient(dataPath)));
    switch (accountType) {
      case USER:
        initWikiFromFlags(WikiFlags(FRENCH_WIKIPEDIA_BOT), *this);
        break;
      case SYSOP:
        initWikiFromFlags(WikiFlags(FRENCH_WIKIPEDIA_SYSOP), *this);
        break;
    }
  } else {
    setHTTPClient(unique_ptr<cbl::HTTPClient>(new RecordReaderHTTPClient(dataPath)));
    m_wikiURL = "https://fr.wikipedia.org/w";
    setInternalUserName("Test user");
    m_siteInfo.fromAPIResponse(json::parse(R"({
        "namespaces": {
          "0": { "id": 0, "case": "first-letter", "*": "" },
          "4": { "id": 4, "case": "first-letter", "*": "Wikipédia" }
        },
        "namespacealiases": [],
        "interwikimap": [],
        "magicwords": []
    })"));
  }
  m_apiLimit = REPLAY_WIKI_API_LIMIT;
  m_apiTitlesLimit = REPLAY_WIKI_API_TITLES_LIMIT;
}

void ReplayWiki::startTestCase(const string& name) {
  static_cast<RecordHTTPClient&>(httpClient()).startTestCase(name);
}

void ReplayWiki::endTestCase() {
  static_cast<RecordHTTPClient&>(httpClient()).endTestCase();
}

TestCaseRecord::TestCaseRecord(ReplayWiki& wiki, const string& name) : m_wiki(wiki) {
  m_wiki.startTestCase(name);
}

TestCaseRecord::~TestCaseRecord() {
  m_wiki.endTestCase();
}

}  // namespace mwc
