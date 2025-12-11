#include <ctime>
#include <print>

#include "logging.h"

auto logging::error(std::string_view message) -> void {
    log(level_prefix<Error>(), message);
}

auto logging::log(std::string_view prefix, std::string_view message) -> void {
    const auto now = std::time({});

    std::array<char, std::size("YYYY-MM-DDTHH:MM:SSZ")> buff;
    std::strftime(buff.data(), buff.size(), "%FT%TZ", std::gmtime(&now));

    std::println(
        stderr,
        "{} {} - {}",
        prefix,
        buff.data(),
        message
    );
}
