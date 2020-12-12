#ifndef CBL_ERROR_H
#define CBL_ERROR_H

#include <functional>
#include <stdexcept>
#include <string>

namespace cbl {

// Base class for all exceptions in the cbl namespace.
// Should not be catched.
class Error : public std::runtime_error {
public:
  using runtime_error::runtime_error;
};

// Generic non-recoverage error, not due to the client.
// Should not be catched.
class InternalError : public Error {
public:
  using Error::Error;
};

// Non-recoverable error due to a logical error on the client side (function call breaking some preconditions).
// Should not be catched.
class InvalidStateError : public Error {
public:
  using Error::Error;
};

// Error of a system call.
class SystemError : public Error {
public:
  using Error::Error;
};

// Some file was not found.
class FileNotFoundError : public SystemError {
public:
  using SystemError::SystemError;
};

// The client is not allowed to execute an operation or access some file.
class PermissionError : public SystemError {
public:
  using SystemError::SystemError;
};

// Invalid string input.
class ParseError : public Error {
public:
  using Error::Error;
};

// Helper class to simulate finally blocks.
class RunOnDestroy {
public:
  RunOnDestroy(const std::function<void()>& f) : m_function(f) {}
  ~RunOnDestroy() { m_function(); }

private:
  std::function<void()> m_function;
};

std::string getCErrorString(int errorNumber);

}  // namespace cbl

#endif
