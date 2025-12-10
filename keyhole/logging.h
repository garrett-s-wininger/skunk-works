#ifndef LOGGING_H
#define LOGGING_H

#include <string_view>

namespace logging {

enum Level {
    Error
};

template<Level l>
consteval const char* level_prefix() {
    if (l == Level::Error) return "[E]";

    return "[U]";
}

auto error(std::string_view message) -> void;
auto log(std::string_view prefix, std::string_view message) -> void;

}

#endif // LOGGING_H
