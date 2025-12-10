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

        std::println("Class File Overview:");

        std::println(
            "  Name - {} ({})",
            class_file.name(),
            class_file.super_class()
        );

        std::println(
            "  Version - {}.{} (Java {})",
            major,
            class_file.minor_version(),
            bytecode::jdk_version(major)
        );

        // NOTE(garrett): The ACC_SUPER flag is enabled by default in JDK 8+ and
        // assumed to be in effect, regardless of actual flags in the Class File
        // (JVMLS Table 4.1-B)
        std::println(
            "  Access Flags - 0x{:04X}",
            class_file.access_flags() ^ static_cast<uint16_t>(bytecode::AccessFlag::Super)
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

        const auto methods = class_file.method_entries();

        if (methods.size() > 0) {
            std::println("Available Methods:");

            for (const auto& method : methods) {
                std::println("  {}", class_file.method_name(method));
            }
        }

        const auto attributes = class_file.attribute_entries();

        if (attributes.size() > 0) {
            std::println("Assigned Attributes: {}", attributes.size());
        }
    } catch (const std::runtime_error& e) {
        logging::error(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
