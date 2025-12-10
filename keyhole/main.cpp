#include <print>

#include "bytecode.h"
#include "logging.h"

using namespace std::literals;

auto help() -> void {
    std::println(stderr, "keyhole CLI v0.1.0\n");
    std::println(stderr, "Provides introspection and instrumentation for JVM bytecode\n");
    std::println(stderr, "USAGE");
    std::println(stderr, "  $ keyhole FILE\n");
    std::println(stderr, "Flags:");
    std::println(stderr, "  -h, --help print this message");
}

auto usage() -> void {
    std::println(stderr, "Usage:");
    std::println(stderr, "  keyhole (-h|--help)");
    std::println(stderr, "  keyhole file");
}

auto main(int argc, char** argv) -> int {
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

    try {
        bytecode::ClassFile class_file{target};
        auto major = class_file.major_version();

        std::println("ClassFile Version:");

        std::println(
            "  {}.{} (Java {})",
            major,
            class_file.minor_version(),
            bytecode::jdk_version(major)
        );

        const auto entries = class_file.constant_pool_entries();

        if (entries.size() > 0) {
            std::println("UTF-8 Constant Pool Entries:");

            for (auto i = 0uz; i < entries.size(); ++i) {
                if (std::holds_alternative<bytecode::ConstantPoolUTF8>(entries[i])) {
                    const auto data = std::get<bytecode::ConstantPoolUTF8>(entries[i]);
                    std::println("  #{}: {}", i + 1, data.text);
                }
            }
        }
    } catch (const std::runtime_error& e) {
        logging::error(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
