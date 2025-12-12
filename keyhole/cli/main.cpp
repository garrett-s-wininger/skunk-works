#include <print>
#include <span>

#include "bytecode.h"
#include "classfile.h"
#include "logging.h"

using namespace std::literals;

auto root_help() -> void {
    std::println(stderr, "keyhole CLI v0.1.0\n");
    std::println(stderr, "Provides introspection and instrumentation for JVM bytecode\n");
    std::println(stderr, "USAGE");
    std::println(stderr, "  $ keyhole inspect - Examines data in .class files");
    std::println(stderr, "  $ keyhole test-class - Writes an example .class file\n");
    std::println(stderr, "Flags:");
    std::println(stderr, "  -h, --help print this message");
}

auto root_usage() -> void {
    std::println(stderr, "Usage:");
    std::println(stderr, "  kh-cli (-h|--help)");
    std::println(stderr, "  kh-cli inspect <ARGS>");
    std::println(stderr, "  kh-cli test-class <ARGS>");
}

auto inspect_class_file(const std::filesystem::path& target) -> void {
    if (!std::filesystem::exists(target)) {
        throw std::runtime_error(
            std::format("Requested file ({}) does not exist", target.string())
        );
    }

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
}

auto write_test_class_file(const std::filesystem::path& target) -> void {
    classfile::ClassFile klass{};
    std::ofstream stream{target};
    classfile::FileSink sink{stream};

    klass.dump_contents(sink);
}

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        root_usage();
        return EXIT_FAILURE;
    }


    if (argv[1] == "-h"sv || argv[1] == "--help"sv) {
        root_help();
        return EXIT_SUCCESS;
    }

    try {
        if (argv[1] == "inspect"sv) {
            inspect_class_file(argv[2]);
        } else if (argv[1] == "test-class"sv) {
            write_test_class_file(argv[2]);
        } else {
            root_usage();
            return EXIT_FAILURE;
        }
    } catch (const std::runtime_error& e) {
        logging::error(e.what());
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}
