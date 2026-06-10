#pragma once

#include <cstddef>

namespace constants {
inline constexpr size_t kIndexPathStart = 48;  // eg: "100644 <40-char-sha> "
inline constexpr size_t kIndexSHA1Start = 7;   // after "100644 "
inline constexpr size_t kIndexSHA1Len = 40;
}  // namespace constants