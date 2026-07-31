#pragma once
#include <cstdarg>

enum Topic : unsigned {
  TOPIC_NONE = 0,
  TOPIC_ISSUE = 1 << 0,
  TOPIC_EXEC = 1 << 1,
  TOPIC_WB = 1 << 2,
  TOPIC_COMMIT = 1 << 3,
  TOPIC_LSQ = 1 << 4,
  TOPIC_MEM = 1 << 5,
  TOPIC_ALL = 0xFFFFFFFFu,
};

namespace debug {
bool enabled(Topic topic);
void print(const char *fmt, ...);
} // namespace debug

#define DPRINT(topic, ...)                                                     \
  do {                                                                         \
    if (::debug::enabled(topic)) {                                             \
      ::debug::print(__VA_ARGS__);                                             \
    }                                                                          \
  } while (0)
