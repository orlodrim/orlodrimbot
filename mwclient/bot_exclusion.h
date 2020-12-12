#ifndef MWC_BOT_EXCLUSION_H
#define MWC_BOT_EXCLUSION_H

#include <string_view>

namespace mwc {

// Checks if the presence of {{nobots}} or {{bots}} blocks an edit.
bool testBotExclusion(std::string_view code, std::string_view bot, std::string_view messageType);

}  // namespace mwc

#endif
