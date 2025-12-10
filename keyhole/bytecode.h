#ifndef BYTECODE_H
#define BYTECODE_H

#include <bit>
#include <cstdint>
#include <fstream>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

namespace bytecode {

constexpr uint32_t CLASS_FILE_MAGIC_VALUE = 0xCAFEBABE;

template <typename T>
concept JVMLSMultiByte =
    std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>;

template <JVMLSMultiByte T>
auto read_multi_byte_value(std::ifstream& content_stream) -> T {
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

auto make_constant_pool_entry(
    ConstantPoolTag tag, std::ifstream& content_stream
) -> ConstantPoolEntry;

class ClassFile {
private:
    ClassFileVersion version;
    std::vector<ConstantPoolEntry> constant_pool;
public:
    ClassFile(const std::filesystem::path& path);
    auto constant_pool_entries() const -> std::span<const ConstantPoolEntry>;
    auto major_version() const -> uint16_t;
    auto minor_version() const -> uint16_t;
};

}

#endif // BYTECODE_H
