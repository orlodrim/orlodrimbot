// Module to parse command line arguments and flags.
//
// Example: to extract flags two flags --filter=<string> and --verbose, as well as an argument containing an input file,
// call parseArgs in your main function in this way:
//   int main(int argc, char** argv) {
//     string inputFile;
//     string filter = ".*";  // If the flag is not passed to the binary, the default value will be preserved.
//     bool verbose = false;  // This means that arguments with POD types must always be initialized.
//     cbl::parseArgs(argc, argv, "input", &inputFile, "--filter", &filter, "--verbose", &verbose);
//     ...
//   }
//
// This binary can parse the following command lines:
//   ./binary input.txt                               # Just an input file.
//   ./binary --filter="^foo.*" input.txt             # Input file + --filter flag.
//   ./binary --filter "^foo.*" input.txt             # The equal sign can be replaced by a space.
//   ./binary --filter="^foo.*" --verbose input.txt   # input.txt is still the input file (--verbose takes no value).
//   ./binary -filter="^foo.*" input.txt              # '-' works the same as '--'.
//   ./binary input.txt --filter="^foo.*"             # Flags can be at any position.
//   ./binary --filter "^foo.*" -- --strange-file--   # '--' marks the explicit end of flags.
//   ./binary --help                                  # --help is supported internally, although it can be overridden.
//
// The name of positional arguments is only used to display them in the help message or in error messages.
// Passing a pointer to vector<string> as the last parameter allows to collect an arbitrary number of additional
// arguments. In the simplest form, this gives:
//   vector<string> allPositionalArgs;
//   cbl::parseArgs(argc, argv, "input", &allPositionalArgs);
// Flags can be marked as required, which means that they must be set and their value must be non-empty:
//   cbl::parseArgs(argv, argv, "--output,required", &outputFile);
//
// Adding a initFromFlagValue(const string&, T&) global function allows T to be used as a value for a flag.
#ifndef CBL_ARGS_PARSER_H
#define CBL_ARGS_PARSER_H

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "error.h"

namespace cbl {

// Exception if command line arguments cannot be parsed according to the specification.
// Note: in case of error in the specification, parseArgs throws std::invalid_argument, not FlagParsingError.
class FlagParsingError : public Error {
public:
  using Error::Error;
};

class ArgsParser;

// Interface to collect flags for libaries.
// For instance, a library can define the following class:
//   class MyLibraryFlags : public cbl::FlagsConsumer {
//   public:
//     void declareFlags(cbl::ArgsParser& parser) override { parser.addArgs("--foo", &m_foo); }
//   private:
//     int m_foo = 0;
//   };
//
// Then, the user can call parseArgs with a MyLibraryFlags object and use it to initialize the library:
//   MyLibraryFlags libraryFlags;
//   cbl::parseArgs(argc, argv, &libraryFlags);
//   initializeMyLibrary(libraryFlags);
//
// If declareArgs generates argument names dynamically, make sure that the const char* pointer passed to addArgs remains
// valid until arguments are parsed (for instance, make it a class member).
class FlagsConsumer {
public:
  virtual void declareFlags(ArgsParser& parser) = 0;
};

// Helper class for parseArgs.
class ArgsParser {
public:
  // Defines command line arguments.
  template <typename T, typename... Args>
  void addArgs(const char* name, T* value, Args... args) {
    if (value == nullptr) {
      throw std::invalid_argument("Flag value must not be null");
    }
    addArg(name, value);
    addArgs(args...);
  }

  template <typename... Args>
  void addArgs(FlagsConsumer* consumer, Args... args) {
    if (consumer == nullptr) {
      throw std::invalid_argument("Flag consumer must not be null");
    }
    consumer->declareFlags(*this);
    addArgs(args...);
  }

  void addArgs() {}

  // Parses command line arguments.
  void run(int argc, const char* const* argv);

private:
  using SetFlagCallback = std::function<void(const std::string&)>;
  enum FlagType {
    VALUED_FLAG,
    BOOL_FLAG,
  };
  struct Flag {
    Flag(FlagType type, bool required, int index, const SetFlagCallback& setCallback)
        : type(type), required(required), index(index), setCallback(setCallback) {}
    FlagType type;
    bool required = false;
    int index = 0;
    const char* value = nullptr;
    SetFlagCallback setCallback;
  };
  struct PositionalArg {
    PositionalArg(const char* name, const SetFlagCallback& setCallback) : name(name), setCallback(setCallback) {}
    const char* name;
    const char* value = nullptr;
    SetFlagCallback setCallback;
  };

  // Type-independent version of addArg, taking a callback function to set the value from a string.
  void addArgWithCallback(const char* name, const SetFlagCallback& setCallback, FlagType type = VALUED_FLAG);
  // Calls the setCallback function for all arguments defined on the command line.
  // This is done in a separate step because for flags defined multiple times on the command line, we only call the
  // callback once for the latest value. This also allows basic errors (e.g. non-existing flags) to be reported before
  // doing potentially complex initialization.
  // The order in which callbacks are called is undefined.
  void setAllArgs();
  // Default handler for --help. Prints existing args in a format similar to the one used by the python argparse module.
  [[noreturn]] void printHelp(const std::string& binary) const;

  template <class T>
  void addArg(const char* name, T* value) {
    addArgWithCallback(name, [value](const std::string& rawValue) { initFromFlagValue(rawValue, *value); });
  }
  // String arguments (the simplest override, does not do any parsing).
  void addArg(const char* name, std::string* value);
  // This override is special in the sense that it defines an unlimited number of arguments. It only works for
  // positional arguments and can only be called once, after all individual positional arguments have been defined.
  // In the future, it may be extended to support flags taking comma-separated values.
  void addArg(const char* name, std::vector<std::string>* extraArgs);
  // Integer arguments. Parsing is strict: invalid integers such as "100 000", "10.", or out-of-range values are
  // rejected.
  void addArg(const char* name, int* value);
  // Bool arguments. This is only supported for flags: passing the flag sets the value to true. There is currently no
  // way to explicitly set the value of a bool flag to false.
  void addArg(const char* name, bool* value);

  std::unordered_map<std::string, Flag> m_flags;
  std::vector<PositionalArg> m_positionalArgs;
  std::string m_extraArgsName;
  std::vector<std::string>* m_extraArgs = nullptr;
};

// Parses command line arguments. See the header for details.
template <typename... Args>
void parseArgs(int argc, const char* const* argv, Args... args) {
  ArgsParser parser;
  parser.addArgs(args...);
  parser.run(argc, argv);
}

}  // namespace cbl

#endif
