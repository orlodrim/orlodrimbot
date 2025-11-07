#ifndef DETECT_STANDARD_MESSAGE_H
#define DETECT_STANDARD_MESSAGE_H

#include <string_view>

namespace wikiutil {

struct StandardMessage {
  enum Type {
    NONE,
    AFD,
    DID_YOU_KNOW,
  };
  Type type = NONE;
};

StandardMessage detectStandardMessage(std::string_view section);

}  // namespace wikiutil

#endif
