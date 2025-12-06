#include <array>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
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
T read_multi_byte_value(std::ifstream& content_stream) {
    T data{};

    content_stream.read(reinterpret_cast<char*>(&data), sizeof(T));

    if constexpr (std::endian::native == std::endian::big) {
        return data;
    } else {
        return std::byteswap(data);
    }

    return data;
}

class ClassFile {
private:
    std::ifstream content_stream;
public:
    ClassFile(std::filesystem::path path) {
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

        auto loaded_magic = read_multi_byte_value<uint32_t>(content_stream);

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

        // TODO(garrett): More class file-y things
    }
};

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
    try {
        bytecode::ClassFile checked_file(target);
    } catch (const std::runtime_error& e) {
        logging::error(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
