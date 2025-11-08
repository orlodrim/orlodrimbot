#include "http_client.h"
#include <curl/curl.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "error.h"
#include "file.h"
#include "sha1.h"
#include "string.h"

using std::string;
using std::string_view;
using std::vector;

namespace cbl {

// Class used to manage calls to curl_global_init / curl_global_cleanup.
// Call CurlGlobalState::getInstance() to ensure that curl_global_init has been called and keep a shared_ptr to the
// result as long as you need to use curl.
// curl_global_cleanup is called automatically when both of these conditions are fulfilled:
// - main() has exited.
// - The last shared_ptr to CurlGlobalState has been released (and thus, the last HTTPClient has been destroyed).
// Edge case: it is not safe to call CurlGlobalState::getInstance() after main() has exited.
class CurlGlobalState {
public:
  CurlGlobalState(const CurlGlobalState&) = delete;
  ~CurlGlobalState() { curl_global_cleanup(); }
  CurlGlobalState& operator=(const CurlGlobalState&) = delete;

  static std::shared_ptr<CurlGlobalState> getInstance() {
    static std::shared_ptr<CurlGlobalState> curlGlobalState(new CurlGlobalState);
    return curlGlobalState;
  }

private:
  CurlGlobalState() {
    CURLcode curlInitCode = curl_global_init(CURL_GLOBAL_ALL);
    if (curlInitCode != 0) {
      throw InternalError("curl_global_init() failed with code " + std::to_string(curlInitCode));
    }
  }
};

class CurlHandle {
public:
  CurlHandle() : m_handle(curl_easy_init()) {
    if (m_handle == nullptr) {
      throw InternalError("curl_easy_init() failed");
    }
  }
  CurlHandle(const CurlHandle&) = delete;
  ~CurlHandle() { curl_easy_cleanup(m_handle); }
  CurlHandle& operator=(const CurlHandle&) = delete;

  CURL* handle() { return m_handle; }
  void setNumOpt(CURLoption option, long value) {
    CURLcode code = curl_easy_setopt(m_handle, option, value);
    if (code != CURLE_OK) {
      throw InternalError("curl_easy_setopt(" + std::to_string(option) + ", " + std::to_string(value) + ") failed");
    }
  }
  void setPtrOpt(CURLoption option, const void* value) {
    CURLcode code = curl_easy_setopt(m_handle, option, value);
    if (code != CURLE_OK) {
      throw InternalError("curl_easy_setopt(" + std::to_string(option) + ", pointer) failed");
    }
  }

private:
  CURL* m_handle = nullptr;
};

class SmartSlist {
public:
  SmartSlist() = default;
  SmartSlist(const vector<string>& vec) { for (const string& s : vec) { append(s); } }
  SmartSlist(const SmartSlist&) = delete;
  SmartSlist(SmartSlist&& ssl) { std::swap(m_slist, ssl.m_slist); }
  SmartSlist& operator=(const SmartSlist&) = delete;
  SmartSlist& operator=(SmartSlist&& ssl) { std::swap(m_slist, ssl.m_slist); return *this; }
  ~SmartSlist() { if (m_slist) curl_slist_free_all(m_slist); }
  void append(const string& str) { m_slist = curl_slist_append(m_slist, str.c_str()); }
  curl_slist* get() { return m_slist; }

private:
  curl_slist* m_slist = nullptr;
};

HTTPClient::HTTPClient() {
  m_curlGlobalState = CurlGlobalState::getInstance();
}

HTTPClient::~HTTPClient() {}

CurlHandle& HTTPClient::curlHandle() {
  if (!m_lazyCurlHandle) {
    m_lazyCurlHandle = std::make_unique<CurlHandle>();
    m_lazyCurlHandle->setNumOpt(CURLOPT_FOLLOWLOCATION, 1);
    m_lazyCurlHandle->setNumOpt(CURLOPT_MAXREDIRS, 10);
    m_lazyCurlHandle->setNumOpt(CURLOPT_TIMEOUT, 300);
    m_lazyCurlHandle->setPtrOpt(CURLOPT_ACCEPT_ENCODING, "");  // All encodings supported by curl.
    if (!m_userAgent.empty()) {
      curlHandle().setPtrOpt(CURLOPT_USERAGENT, m_userAgent.c_str());
    }
    if (m_remoteCookiesEnabled) {
      m_lazyCurlHandle->setPtrOpt(CURLOPT_COOKIEFILE, "");
    } else if (!m_cookies.empty()) {
      curlHandle().setPtrOpt(CURLOPT_COOKIE, m_cookies.c_str());
    }
  }
  return *m_lazyCurlHandle;
}

void HTTPClient::resetCurlHandle() {
  m_lazyCurlHandle.reset();
}

size_t HTTPClient::callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
  return ((HTTPClient*) userdata)->write(ptr, size, nmemb);
}

size_t HTTPClient::write(void* ptr, size_t size, size_t nmemb) {
  size_t oldSize = m_buffer->size();
  size_t recvSize = size * nmemb;
  if (recvSize != 0) {
    m_buffer->resize(oldSize + recvSize);
    memcpy(&(*m_buffer)[oldSize], ptr, recvSize);
  }
  return recvSize;
}

string HTTPClient::openInternal(const string& url, const char* errorMessagePrefix) {
  if (m_delayBeforeRequests > 0) {
    sleep(m_delayBeforeRequests);
  }
  string content;
  m_buffer = &content;
  curlHandle().setPtrOpt(CURLOPT_WRITEFUNCTION, (const void*) HTTPClient::callback);
  curlHandle().setPtrOpt(CURLOPT_WRITEDATA, this);
  curlHandle().setPtrOpt(CURLOPT_URL, url.c_str());
  CURLcode perfCode = curl_easy_perform(curlHandle().handle());
  if (perfCode != CURLE_OK) {
    throw NetworkError(string(errorMessagePrefix) + " '" + url + "': curl_easy_perform failed with code " +
                       std::to_string(perfCode));
  }
  long httpCode = 0;
  perfCode = curl_easy_getinfo(curlHandle().handle(), CURLINFO_RESPONSE_CODE, &httpCode);
  if (perfCode != CURLE_OK) {
    throw InternalError("curl_easy_getinfo failed");
  }
  if (httpCode != 200 && httpCode != 202) {
    string errorMessage =
        string(errorMessagePrefix) + " '" + url + "': server returned HTTP error " + std::to_string(httpCode);
    if (httpCode == 403) {
      throw HTTPForbiddenError(httpCode, errorMessage);
    } else if (httpCode == 404) {
      throw HTTPNotFoundError(httpCode, errorMessage);
    } else if (httpCode >= 500 && httpCode < 600) {
      throw HTTPServerError(httpCode, errorMessage);
    } else {
      throw HTTPError(httpCode, errorMessage);
    }
  }
  return content;
}

string HTTPClient::get(const string& url) {
  SmartSlist slist(m_headers);
  curlHandle().setPtrOpt(CURLOPT_HTTPHEADER, slist.get());
  RunOnDestroy cleanup([this]() {
    curlHandle().setPtrOpt(CURLOPT_HTTPHEADER, nullptr);
  });
  return openInternal(url, "Cannot read");
}

string HTTPClient::post(const string& url, const string& data) {
  string content;
  SmartSlist slist(m_headers);
  slist.append("Expect:");
  curlHandle().setPtrOpt(CURLOPT_HTTPHEADER, slist.get());
  curlHandle().setNumOpt(CURLOPT_POST, 1);
  curlHandle().setPtrOpt(CURLOPT_POSTFIELDS, data.c_str());
  RunOnDestroy cleanup([this]() {
    curlHandle().setPtrOpt(CURLOPT_POSTFIELDS, "");
    curlHandle().setNumOpt(CURLOPT_POST, 0);
    curlHandle().setPtrOpt(CURLOPT_HTTPHEADER, nullptr);
  });
  return openInternal(url, "Failure of POST request on");
}

void HTTPClient::setDelayBeforeRequests(int seconds) {
  m_delayBeforeRequests = seconds;
}

const string& HTTPClient::userAgent() const {
  return m_userAgent;
}

void HTTPClient::setUserAgent(const string& value) {
  if (m_userAgent == value) return;
  m_userAgent = value;
  resetCurlHandle();
}

const string& HTTPClient::cookies() const {
  return m_cookies;
}

void HTTPClient::setCookies(string_view cookies) {
  m_cookies = cookies;
  resetCurlHandle();
}

void HTTPClient::clearCookies() {
  setCookies("");
}

void HTTPClient::setRemoteCookiesEnabled(bool value) {
  if (m_remoteCookiesEnabled == value) return;
  m_remoteCookiesEnabled = value;
  resetCurlHandle();
}

string HTTPClient::getRemoteCookies() const {
  string cookies;
  if (!m_lazyCurlHandle) return cookies;

  curl_slist* slist = nullptr;
  CURLcode code = curl_easy_getinfo(m_lazyCurlHandle->handle(), CURLINFO_COOKIELIST, &slist);
  if (code != CURLE_OK) {
    throw InternalError("curl_easy_getinfo(CURLINFO_COOKIELIST) failed");
  }

  for (curl_slist* p = slist; p != nullptr; p = p->next) {
    if (!cookies.empty()) cookies += "; ";
    vector<string_view> fields = splitAsVector(p->data, '\t');
    if (fields.size() >= 7) {
      cookies += fields[5];
      cookies += '=';
      cookies += fields[6];
    }
  }

  if (slist) {
    curl_slist_free_all(slist);
  }
  return cookies;
}

string HTTPClientWithCache::getCacheFileForGET(const string& url) const {
  return getCacheFile(url);
}

string HTTPClientWithCache::getCacheFileForPOST(const string& url, const string& data) const {
  return getCacheFile(url + '\n' + data);
}

string HTTPClientWithCache::get(const string& url) {
  string content;
  if (m_cacheMode & CACHE_ENABLED) {
    m_lastCacheFile = getCacheFileForGET(url);
    if ((m_cacheMode & CACHE_READ_ENABLED) && fileExists(m_lastCacheFile)) {
      try {
        return readFile(m_lastCacheFile);
      } catch (const cbl::Error& e) {
        throw cbl::InternalError("Reading " + url + " from cache failed: " + e.what());
      }
    } else if (m_cacheMode & CACHE_OFFLINE_MODE) {
      throw PageNotInCacheError(url + " is not in cache");
    }
  } else {
    m_lastCacheFile.clear();
  }
  content = HTTPClient::get(url);
  if (m_cacheMode & CACHE_WRITE_ENABLED) {
    writeFile(m_lastCacheFile, content);
  }
  return content;
}

string HTTPClientWithCache::post(const string& url, const string& data) {
  string content;
  if (m_cacheMode & CACHE_ENABLED) {
    m_lastCacheFile = getCacheFileForPOST(url, data);
    if (!(m_cacheMode & CACHE_POST)) {
      throw InvalidStateError("Attempt to cache result of POST request on " + url +
                              " while the cache of POST requests is disabled");
    } else if ((m_cacheMode & CACHE_READ_ENABLED) && fileExists(m_lastCacheFile)) {
      try {
        return readFile(m_lastCacheFile);
      } catch (const cbl::Error& e) {
        throw cbl::InternalError("Reading cached POST request to " + url + " failed: " + e.what());
      }
    } else if (m_cacheMode & CACHE_OFFLINE_MODE) {
      throw PageNotInCacheError(url + " + data for POST request are not in cache");
    }
  } else {
    m_lastCacheFile.clear();
  }
  content = HTTPClient::post(url, data);
  if (m_cacheMode & CACHE_WRITE_ENABLED) {
    writeFile(m_lastCacheFile, content);
  }
  return content;
}

string HTTPClientWithCache::getCacheFile(const string& request) const {
  if (m_cacheDir.empty()) {
    throw InvalidStateError("HTTPClientWithCache::getCacheFile() called without initializing the cache directory");
  }
  string cacheFile = m_cacheDir;
  if (cacheFile[cacheFile.size() - 1] != '/') cacheFile += '/';
  cacheFile += sha1(request);
  cacheFile += ".dat";
  return cacheFile;
}

int HTTPClientWithCache::cacheMode() const {
  return m_cacheMode;
}

void HTTPClientWithCache::setCacheMode(int mode) {
  if ((mode & CACHE_ENABLED) == 0 && mode != 0) {
    throw std::invalid_argument(
        "Bad mode for HTTPClientWithCache::setCacheMode: if read and write cache are disabled, no other flags can be "
        "enabled");
  } else if ((mode & CACHE_OFFLINE_MODE) && !(mode & CACHE_READ_ENABLED)) {
    throw std::invalid_argument(
        "Bad mode for HTTPClientWithCache::setCacheMode: offline mode requires that reading from cache is enabled");
  }
  m_cacheMode = mode;
}

const string& HTTPClientWithCache::cacheDir() const {
  return m_cacheDir;
}

void HTTPClientWithCache::setCacheDir(const string& dir) {
  m_cacheDir = dir;
}

void HTTPClientWithCache::doNotCacheLastResponse() {
  if (!m_lastCacheFile.empty()) {
    removeFile(m_lastCacheFile, /* mustExist = */ false);
    m_lastCacheFile.clear();
  }
}

}  // namespace cbl
