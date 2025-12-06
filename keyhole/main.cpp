#include <array>
#include <ctime>
#include <filesystem>
#include <format>
#include <print>
#include <string>
#include <string_view>

using namespace std::literals;

// TODO(garrett): Separate out when larger/more concrete
namespace logging {

enum Level {
    Error
};

template<Level l>
consteval const char* level_prefix() {
    if (l == Level::Error) return "[E]";

    return "[U]";
}

void log(std::string_view prefix, std::string_view message) {
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

void error(std::string_view message) {
    log(level_prefix<Error>(), message);
}

}

void help() {
    std::println(stderr, "keyhole CLI v0.1.0\n");
    std::println(stderr, "Provides introspection and instrumentation for JVM bytecode\n");
    std::println(stderr, "USAGE");
    std::println(stderr, "  $ keyhole FILE\n");
    std::println(stderr, "Flags:");
    std::println(stderr, "  -h, --help print this message");
}

void usage() {
    std::println(stderr, "Usage:");
    std::println(stderr, "  keyhole (-h|--help)");
    std::println(stderr, "  keyhole file");
}

int main(int argc, char** argv) {
    if (argc != 2) {
        usage();
        return EXIT_FAILURE;
    }

    std::string_view requested_target{argv[1]};

    if (requested_target == "-h"sv || requested_target == "--help"sv) {
        help();
        return EXIT_SUCCESS;
    }

    std::filesystem::path target{requested_target};

    if (!std::filesystem::exists(target)) {
        logging::error(
            std::format("Requested file ({}) does not exist", requested_target)
        );

        return EXIT_FAILURE;
    }

    // TODO(garrett): File processing

    return EXIT_SUCCESS;
}
