#include "args_parser.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "error.h"
#include "string.h"

using std::string;
using std::unordered_map;
using std::vector;

namespace cbl {

// The only thing that matters for bool flags is whether their value is null or not null.
// This is used to provide an arbitrary non-null pointer.
const char ARBITRARY_VALUE_FOR_TRUE_BOOL[] = "1";

// An argument is a flag if it starts with "-", unless it is equal to "-" or it is a negative integer (e.g. "-123").
// The exception "-" is useful because it is a common value to indicate "standard input".
// Note that the function returns true for "--", which has another special meaning (explicit end of flags).
static bool isFlagArg(const char* arg) {
  if (*arg != '-') return false;
  arg++;
  for (; *arg >= '0' && *arg <= '9'; arg++) {}
  return *arg != '\0';
}

static bool mayBeConfusedWithFlagArg(const char* arg) {
  return *arg == '-';
}

// Extracts the flag name from arg and sets endOfName to the end of the flag name (first '\0' or ',' or '=').
// Precondition: isFlagArg(arg) must be true.
static string parseFlagName(const char* arg, const char** endOfName = nullptr) {
  const char* start = arg + 1;
  if (*start == '-') {
    start++;
  }
  const char* end = start;
  for (; *end && *end != '=' && *end != ','; end++) {}
  if (endOfName) {
    *endOfName = end;
  }
  return string(start, end - start);
}

// Returns the flag in upper case, for use as a placeholder value in the message displayed by --help.
static string getPlaceholderValueForFlag(const string& flagName) {
  string value = flagName;
  for (char& c : value) {
    if (c >= 'a' && c <= 'z') {
      c += 'A' - 'a';
    } else if (c == '-') {
      c = '_';
    }
  }
  return value;
}

void ArgsParser::addArgWithCallback(const char* name, const SetFlagCallback& setCallback, FlagType type) {
  if (isFlagArg(name)) {
    const char* endOfName = nullptr;
    string flagName = parseFlagName(name, &endOfName);
    if ((*endOfName != '\0' && *endOfName != ',') || flagName.empty()) {
      throw std::invalid_argument("Invalid flag name '" + string(name) + "'");
    } else if (flagName[0] == '-') {
      throw std::invalid_argument("Too many dashes in flag name '" + string(name) + "'");
    }
    bool required = false;
    if (*endOfName == ',') {
      string attributes(endOfName + 1);
      if (attributes == "required") {
        required = true;
      } else {
        throw std::invalid_argument("Invalid flag attribute '" + attributes + "'");
      }
    }
    if (!m_flags.emplace(std::move(flagName), Flag(type, required, m_flags.size(), setCallback)).second) {
      throw std::invalid_argument("Duplicate flag '" + string(name) + "'");
    }
  } else if (mayBeConfusedWithFlagArg(name)) {
    // Things that start with '-' but are not recognized as flags by isFlagArg (such as negative numbers) are not
    // allowed as names of positional arguments either, because this would be confusing.
    throw std::invalid_argument("Invalid flag name '" + string(name) + "'");
  } else if (type == BOOL_FLAG) {
    throw std::invalid_argument("Positional argument '" + string(name) + "' cannot have a boolean value");
  } else if (m_extraArgs) {
    throw std::invalid_argument("Positional argument '" + string(name) +
                                "' cannot be declared after the catch-all vector of positional arguments");
  } else {
    m_positionalArgs.emplace_back(name, setCallback);
  }
}

void ArgsParser::addArg(const char* name, string* value) {
  addArgWithCallback(name, [value](const string& rawValue) { *value = rawValue; });
}

void ArgsParser::addArg(const char* name, int* value) {
  addArgWithCallback(name, [value](const string& rawValue) { *value = parseInt(rawValue); });
}

void ArgsParser::addArg(const char* name, bool* value) {
  addArgWithCallback(
      name, [value](const string&) { *value = true; }, BOOL_FLAG);
}

void ArgsParser::addArg(const char* name, vector<string>* extraArgs) {
  if (isFlagArg(name) || mayBeConfusedWithFlagArg(name)) {
    throw std::invalid_argument("Flag values cannot be parsed to vector<string>");
  } else if (m_extraArgs != nullptr) {
    throw std::invalid_argument("Multiple catch-all vectors of positional arguments provided");
  } else {
    m_extraArgsName = name;
    m_extraArgs = extraArgs;
  }
}

void ArgsParser::setAllArgs() {
  for (const auto& [name, flag] : m_flags) {
    if (flag.required) {
      if (flag.value == nullptr) {
        throw FlagParsingError("Missing required flag --" + name);
      } else if (flag.value[0] == '\0') {
        throw FlagParsingError("Empty value for required flag --" + name);
      }
    }
    if (flag.value != nullptr) {
      flag.setCallback(flag.value);
    }
  }
  for (const PositionalArg& arg : m_positionalArgs) {
    if (arg.value == nullptr) {
      throw InternalError("Uninitialized argument '" + string(arg.name) + "'");
    }
    arg.setCallback(arg.value);
  }
}

void ArgsParser::printHelp(const string& binary) const {
  size_t lastSlash = binary.rfind('/');
  string invocation = "Usage: " + (lastSlash == string::npos ? binary : binary.substr(lastSlash + 1));
  vector<string> chunks;

  using FlagIt = unordered_map<string, Flag>::const_iterator;
  vector<FlagIt> sortedFlags;
  for (FlagIt it = m_flags.begin(); it != m_flags.end(); ++it) {
    sortedFlags.push_back(it);
  }
  // Sorts flags in the order they were defined.
  std::sort(sortedFlags.begin(), sortedFlags.end(),
            [](FlagIt it1, FlagIt it2) { return it1->second.index < it2->second.index; });
  for (FlagIt flagIt : sortedFlags) {
    const string& name = flagIt->first;
    const Flag& flag = flagIt->second;
    string flagDescription;
    if (!flag.required) {
      flagDescription += '[';
    }
    flagDescription += "--";
    flagDescription += name;
    if (flag.type == VALUED_FLAG) {
      flagDescription += '=';
      flagDescription += getPlaceholderValueForFlag(name);
    }
    if (!flag.required) {
      flagDescription += ']';
    }
    chunks.push_back(std::move(flagDescription));
  }
  for (const PositionalArg& arg : m_positionalArgs) {
    chunks.push_back(arg.name);
  }
  if (m_extraArgs) {
    chunks.push_back("[" + m_extraArgsName + " [" + m_extraArgsName + " ...]]");
  }

  // Formats the output with 80 characters per line.
  int positionOnLine = invocation.size();
  int indentation;
  if (positionOnLine < 40) {
    indentation = positionOnLine + 1;
  } else {
    positionOnLine += 100;
    indentation = 7;
  }
  std::cerr << invocation;
  for (const string& chunk : chunks) {
    if (positionOnLine + 1 + chunk.size() > 80) {
      std::cerr << '\n' << string(indentation, ' ');
      positionOnLine = indentation;
    } else {
      std::cerr << ' ';
      positionOnLine++;
    }
    std::cerr << chunk;
    positionOnLine += chunk.size();
  }
  std::cerr << '\n';
  exit(0);
}

void ArgsParser::run(int argc, const char* const* argv) {
  // First step: sets the 'value' member of flags and positional arguments.
  bool endOfFlags = false;
  int numPositionalArgs = 0;
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (!endOfFlags && isFlagArg(arg)) {
      const char* endOfName = nullptr;
      string flagName = parseFlagName(arg, &endOfName);
      if (*endOfName != '\0' && *endOfName != '=') {
        throw FlagParsingError("Invalid flag " + string(arg));
      } else if (flagName.empty()) {
        // An empty flag "--" marks the explicit end of flags.
        endOfFlags = true;
      } else {
        unordered_map<string, Flag>::iterator flagIt = m_flags.find(flagName);
        if (flagIt != m_flags.end()) {
          Flag& flag = flagIt->second;
          switch (flag.type) {
            case VALUED_FLAG:
              // Two syntaxes are supported: "--someflag=value" or "--someflag value".
              if (*endOfName == '=') {
                flag.value = endOfName + 1;
              } else if (i + 1 < argc && !isFlagArg(argv[i + 1])) {
                flag.value = argv[i + 1];
                i++;
              } else {
                throw FlagParsingError("Missing value for flag --" + flagName);
              }
              break;
            case BOOL_FLAG:
              if (*endOfName == '=') {
                throw FlagParsingError("Flag --" + flagName + " does not take a value");
              }
              flag.value = ARBITRARY_VALUE_FOR_TRUE_BOOL;
              break;
          }
        } else if (flagName == "help") {
          printHelp(argv[0]);
        } else {
          throw FlagParsingError("Invalid flag --" + flagName);
        }
      }
    } else if (numPositionalArgs < static_cast<int>(m_positionalArgs.size())) {
      m_positionalArgs[numPositionalArgs].value = arg;
      numPositionalArgs++;
    } else if (m_extraArgs) {
      m_extraArgs->push_back(arg);
    } else {
      throw FlagParsingError("Too many arguments");
    }
  }
  if (numPositionalArgs < static_cast<int>(m_positionalArgs.size())) {
    throw FlagParsingError("Missing argument " + string(m_positionalArgs[numPositionalArgs].name));
  }

  // Second step: call callbacks to parse values.
  setAllArgs();
}

}  // namespace cbl
