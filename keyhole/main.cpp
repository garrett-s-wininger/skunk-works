#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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

// TODO(garrett): Separate out when larger/more concrete
namespace bytecode {

constexpr uint32_t CLASS_FILE_MAGIC_VALUE = 0xCAFEBABE;

template <typename T>
concept JVMLSMultiByte =
    std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>;

template <JVMLSMultiByte T>
auto read_multi_byte_value(std::ifstream& content_stream) -> T{
    T data{};

    content_stream.read(reinterpret_cast<char*>(&data), sizeof(T));

    if constexpr (std::endian::native == std::endian::big) {
        return data;
    } else {
        return std::byteswap(data);
    }

    return data;
}

constexpr auto jdk_version(const uint16_t bytecode_major_version) -> uint8_t {
    if (bytecode_major_version < 49) {
        // NOTE(garrett): Bundle 1.0-1.4 just because we're being lazy here.
        return 1;
    }

    // NOTE(garrett): Starting at JDK 5, there's a 44 number wide gap between
    // the major version and the named release number.
    return bytecode_major_version - 44;
}

struct ClassFileVersion {
    uint16_t major_version;
    uint16_t minor_version;
};

enum class ConstantPoolTag : uint8_t {
    UTF8 = 1,
    Class = 7,
    MethodReference = 10,
    NameAndType = 12
};

constexpr std::array<ConstantPoolTag, 4> CLASS_FILE_CONSTANT_POOL_TAGS {
    ConstantPoolTag::UTF8,
    ConstantPoolTag::Class,
    ConstantPoolTag::MethodReference,
    ConstantPoolTag::NameAndType
};

struct ConstantPoolClass {
    uint16_t name_index;
};

struct ConstantPoolMethodReference {
    uint16_t class_index;
    uint16_t name_and_type_index;
};

struct ConstantPoolNameAndType {
    uint16_t name_index;
    uint16_t descriptor_index;
};

struct ConstantPoolUTF8 {
    // NOTE(garrett): Going for simplicity, this is NOT proper unicode handling
    std::string text;
};

using ConstantPoolEntry = std::variant<
    ConstantPoolClass,
    ConstantPoolMethodReference,
    ConstantPoolNameAndType,
    ConstantPoolUTF8
>;

auto make_constant_pool_entry(ConstantPoolTag tag, std::ifstream& content_stream) -> ConstantPoolEntry {
    // FIXME(garrett): One of these is parsing extra or too-little data
    switch (tag) {
        case ConstantPoolTag::Class: {
            auto data = ConstantPoolClass{};
            data.name_index = read_multi_byte_value<uint16_t>(content_stream);
            return data;
        }
        case ConstantPoolTag::MethodReference: {
            auto data = ConstantPoolMethodReference{};
            data.class_index = read_multi_byte_value<uint16_t>(content_stream);
            data.name_and_type_index = read_multi_byte_value<uint16_t>(content_stream);
            return data;
        }
        case ConstantPoolTag::NameAndType: {
            auto data = ConstantPoolNameAndType{};
            data.name_index = read_multi_byte_value<uint16_t>(content_stream);
            data.descriptor_index = read_multi_byte_value<uint16_t>(content_stream);
            return data;
        }
        case ConstantPoolTag::UTF8: {
            auto data = ConstantPoolUTF8{};
            auto string_length = read_multi_byte_value<uint16_t>(content_stream);

            data.text = std::string(string_length, '\0');
            content_stream.read(data.text.data(), data.text.capacity());

            return data;
        }
    }

    throw std::logic_error("Exhausted tag implementations or fell through");
}

class ClassFile {
private:
    ClassFileVersion version;
    std::vector<ConstantPoolEntry> constant_pool;
public:
    ClassFile() : version(ClassFileVersion{55, 0}) {}

    ClassFile(const std::filesystem::path& path) {
        const auto path_string = path.string();

        std::ifstream content_stream(
            path_string,
            std::ios::binary
        );

        if (!content_stream.is_open()) {
            throw std::runtime_error(
                std::format("Unable to open class file ({})", path_string)
            );
        }

        const auto loaded_magic = read_multi_byte_value<uint32_t>(content_stream);

        if (loaded_magic != CLASS_FILE_MAGIC_VALUE) {
            throw std::runtime_error(
                std::format(
                    "Class file ({}) contains invalid magic value: 0x{:08X} != 0x{:08X}",
                    path_string,
                    loaded_magic,
                    CLASS_FILE_MAGIC_VALUE
                )
            );
        }

        // NOTE(garrett): JVMLS states the class file format is minor and then
        // major, section 4.1 - The ClassFile Structure
        version = ClassFileVersion{};
        version.minor_version = read_multi_byte_value<uint16_t>(content_stream);
        version.major_version = read_multi_byte_value<uint16_t>(content_stream);

        const auto constant_pool_count = read_multi_byte_value<uint16_t>(content_stream);
        constant_pool = std::vector<ConstantPoolEntry>(constant_pool_count);

        for (auto i = 0; i < constant_pool_count; ++i) {
            uint8_t tag{};
            content_stream.read(reinterpret_cast<char*>(&tag), sizeof(tag));

            auto is_valid = std::ranges::any_of(
                CLASS_FILE_CONSTANT_POOL_TAGS,
                [=](const auto known_tag) {
                    return static_cast<uint8_t>(known_tag) == tag;
                }
            );

            if (!is_valid) {
                throw std::runtime_error(
                    std::format("Encountered unknown tag value: {}", tag)
                );
            }

            auto entry = make_constant_pool_entry(
                static_cast<ConstantPoolTag>(tag),
                content_stream
            );

            constant_pool.push_back(entry);

            // TODO(garrett): Remove, more constant pool parsing
            std::visit([](auto&& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, ConstantPoolUTF8>) {
                    std::println("UTF-8 Value: {}", value.text);
                }
            }, entry);
        }
    }

    auto major_version() const -> uint16_t {
        return version.major_version;
    }

    auto minor_version() const -> uint16_t {
        return version.minor_version;
    }
};

}

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

        std::println(
            "ClassFile version {}.{} (Java {})",
            major,
            class_file.minor_version(),
            bytecode::jdk_version(major)
        );
    } catch (const std::runtime_error& e) {
        logging::error(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
