#include "bot_exclusion.h"
#include <string_view>
#include "cbl/string.h"

using std::string_view;

namespace mwc {

static bool itemInList(string_view item, string_view values) {
  for (string_view value : cbl::split(values, ',')) {
    string_view trimmedValue = cbl::trim(value);
    if (trimmedValue == item || trimmedValue == "all") return true;
  }
  return false;
}

bool testBotExclusion(string_view code, string_view bot, string_view messageType) {
  while (true) {
    size_t templateBegin = code.find("{{");
    if (templateBegin == string_view::npos) break;
    templateBegin += 2;
    size_t templateNameEnd = code.find_first_of("|{}", templateBegin);
    if (templateNameEnd == string_view::npos) break;
    size_t templateEnd = code.find_first_of("{}", templateNameEnd);
    if (templateEnd == string_view::npos) break;
    string_view templateName = cbl::trim(code.substr(templateBegin, templateNameEnd - templateBegin));
    if (templateName == "Nobots" || templateName == "nobots") {
      return true;
    } else if ((templateName == "Bots" || templateName == "bots") && templateNameEnd < templateEnd) {
      string_view fields = code.substr(templateNameEnd + 1, templateEnd - templateNameEnd - 1);
      for (string_view field : cbl::split(fields, '|')) {
        size_t equalPosition = field.find('=');
        if (equalPosition == string_view::npos) continue;
        string_view param = cbl::trim(field.substr(0, equalPosition));
        string_view values = field.substr(equalPosition + 1);
        if ((param == "allow" && !itemInList(bot, values)) || (param == "deny" && itemInList(bot, values)) ||
            (param == "optout" && !messageType.empty() && itemInList(messageType, values))) {
          return true;
        }
      }
    }
    code.remove_prefix(templateEnd);
  }
  return false;
}

}  // namespace mwc
