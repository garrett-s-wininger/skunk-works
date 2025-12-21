#include <filesystem>
#include <print>
#include <span>

#include "classfile.h"
#include "logging.h"
#include "sinks.h"

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

constexpr auto jdk_version(const classfile::Version version) -> uint8_t {
    if (version.major < 49) {
        // NOTE(garrett): Bundle 1.0-1.4 just because we're being lazy here.
        return 1;
    }

    // NOTE(garrett): Starting at JDK 5, there's a 44 number wide gap between
    // the major version and the named release number.
    return version.major - 44;
}

auto inspect_class_file(const std::filesystem::path& target) -> void {
    if (!std::filesystem::exists(target)) {
        throw std::runtime_error(
            std::format("Requested file ({}) does not exist", target.string())
        );
    }

    std::ifstream file_reader{target, std::ios::binary | std::ios::ate};

    if (!file_reader.is_open()) {
        throw std::runtime_error(
            std::format("Failed to open file ({})", target.string())
        );
    }

    const auto size = file_reader.tellg();
    file_reader.seekg(0, std::ios::beg);

    std::vector<std::byte> contents{};
    contents.resize(size);

    if (!file_reader.read(reinterpret_cast<char*>(contents.data()), size)) {
        throw std::runtime_error(
            std::format("Failed to read file ({}) contents", target.string())
        );
    }

    reader::Reader reader{contents};
    const auto class_file = classfile::ClassFile::parse(reader);

    if (!class_file) {
        throw std::runtime_error(
            std::format("Failed to parse file ({}) contents", target.string())
        );
    }

    const auto klass = class_file.value();

    std::println("Class File Overview:");

    std::println(
        "  Name         - {} ({})",
        klass.name(),
        klass.superclass()
    );

    std::println(
        "  Version      - {}.{} (Java {})",
        klass.version().major,
        klass.version().minor,
        jdk_version(klass.version())
    );

    std::println(
        "  Access Flags - 0x{:04X}",
        klass.access_flags
    );

    const auto entries = klass.constant_pool.entries();

    if (entries.size() > 0) {
        std::println("Constant Pool Entries:");

        for (auto i = 0uz; i < entries.size(); ++i) {
            std::println(
                "  {:>2}#: [{}]",
                i + 1,
                constant_pool::name(entries[i])
            );
        }
    }

    if (klass.methods.size() > 0) {
        std::println("Available Methods:");

        for (const auto& method : klass.methods) {
            std::println(
                "  {}",
                klass.constant_pool.resolve<constant_pool::UTF8Entry>(
                    method.name_index
                ).text
            );
        }
    }

    if (klass.attributes.size() > 0) {
        std::println("Assigned Attributes:");

        for (const auto& attribute : klass.attributes) {
            std::println(
                "  {}",
                klass.constant_pool.resolve<constant_pool::UTF8Entry>(
                    attribute.name_index
                ).text
            );
        }
    }
}

auto write_test_class_file(const std::filesystem::path& target) -> void {
    classfile::ClassFile klass{"MyClass"sv};
    std::ofstream stream{target};

    if (!stream) {
        throw std::runtime_error("Failed to open requested file");
    }

    sinks::FileSink sink{stream};
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
