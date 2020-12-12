// C++ wrapper for libcurl to do GET and POST requests.
// Usage:
//   HTTPClient client;
//   string response = client.get("https://example.com");
//   client.setRemoteCookiesEnabled(true);
//   string loginResponse = client.post("https://example.com/login", "user=X&password=Y");
#ifndef CBL_HTTP_CLIENT_H
#define CBL_HTTP_CLIENT_H

#include <memory>
#include <string>
#include <string_view>
#include "error.h"

namespace cbl {

class CurlGlobalState;
class CurlHandle;

// No response from an HTTP server (e.g. no Internet connection or invalid domain).
class NetworkError : public Error {
public:
  using Error::Error;
};

// The HTTP server returned an HTTP error.
class HTTPError : public Error {
public:
  HTTPError(int httpCode, const std::string& message) : Error(message), m_httpCode(httpCode) {}
  int httpCode() const { return m_httpCode; }

private:
  int m_httpCode;
};

// HTTP error 404.
class HTTPNotFoundError : public HTTPError {
public:
  using HTTPError::HTTPError;
};

// HTTP error 403.
class HTTPForbiddenError : public HTTPError {
public:
  using HTTPError::HTTPError;
};

// HTTP error 5xx.
class HTTPServerError : public HTTPError {
public:
  using HTTPError::HTTPError;
};

// The server is in mode CACHE_OFFLINE_MODE but the response is not in cache.
class PageNotInCacheError : public Error {
public:
  using Error::Error;
};

// Curl wrapper (see the top of the file).
class HTTPClient {
public:
  HTTPClient();
  virtual ~HTTPClient();

  // Retrieves an URL with a GET request.
  // Throws: HTTPError and subclasses, NetworkError.
  virtual std::string get(const std::string& url);
  // Sends a POST request to the specified URL.
  // Throws: HTTPError and subclasses, NetworkError.
  virtual std::string post(const std::string& url, const std::string& data);

  // If seconds > 0, wait the specified number of seconds before each request.
  void setDelayBeforeRequests(int seconds);
  const std::string& userAgent() const;
  // Sets the value of the User-Agent header. If empty, no User-Agent header is sent (this is the default).
  void setUserAgent(const std::string& value);
  // Get cookies set by setCookies (but not those received from the server).
  const std::string& cookies() const;
  // Set cookies in the format of the Set-Cookie header (e.g. "cookie1=value1; cookie2=value2").
  // Do not use if remote cookies are enabled.
  void setCookies(std::string_view cookies);
  // Remove cookies defined by setCookies. Do not use if remote cookies are enabled.
  void clearCookies();
  // If enabled, keeps cookies received from the server between requests (cookies set with setCookies are always sent
  // on top of that without deduplication, so using both mechanisms at the same time is not recommended).
  // Warning: Calling setUserAgent, setCookies, or this function clears all remote cookies.
  bool remoteCookiesEnabled() const { return m_remoteCookiesEnabled; }
  void setRemoteCookiesEnabled(bool value);
  // Get cookies received from the server (but not those set by setCookies).
  // The returned value has the format of the Set-Cookie header.
  std::string getRemoteCookies() const;

private:
  static size_t callback(void* ptr, size_t size, size_t nmemb, void* userdata);

  CurlHandle& curlHandle();
  void resetCurlHandle();
  // Throws: HTTPForbiddenError, HTTPNotFoundError, HTTPServerError, HTTPError, NetworkError.
  std::string openInternal(const std::string& url, const char* errorMessagePrefix);
  size_t write(void* ptr, size_t size, size_t nmemb);
  void updateCookieLine();

  std::unique_ptr<CurlHandle> m_lazyCurlHandle;
  std::shared_ptr<CurlGlobalState> m_curlGlobalState;

  bool m_remoteCookiesEnabled = false;
  std::string m_cookies;
  int m_delayBeforeRequests = 0;
  std::string m_userAgent;

  std::string* m_buffer = nullptr;
};

}  // namespace cbl

#endif
