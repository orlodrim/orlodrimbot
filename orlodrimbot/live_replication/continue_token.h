#ifndef LIVE_REPLICATION_CONTINUE_TOKEN_H
#define LIVE_REPLICATION_CONTINUE_TOKEN_H

#include <cstdint>
#include <string>
#include <string_view>

namespace live_replication {

int64_t parseContinueToken(std::string_view token, std::string_view expectedType);

std::string buildContinueToken(std::string_view type, int64_t data);

}  // namespace live_replication

#endif
