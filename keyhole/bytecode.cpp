#include <algorithm>
#include <format>

#include "bytecode.h"

auto bytecode::make_constant_pool_entry(ConstantPoolTag tag, std::ifstream& content_stream) -> ConstantPoolEntry {
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
            content_stream.read(data.text.data(), data.text.size());

            return data;
        }
    }

    throw std::logic_error("Exhausted tag implementations or fell through");
}

bytecode::ClassFile::ClassFile(const std::filesystem::path& path) {
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

    // NOTE(garrett): JVMLS 4.1 - The ClassFile Structure, this data returns
    // 1 higher than the actual pool count
    const auto constant_pool_count = read_multi_byte_value<uint16_t>(content_stream);
    const auto real_entries = constant_pool_count - 1;

    constant_pool = std::vector<ConstantPoolEntry>();
    constant_pool.reserve(real_entries);

    for (auto i = 0; i < real_entries; ++i) {
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

        constant_pool.push_back(
            make_constant_pool_entry(
                static_cast<ConstantPoolTag>(tag),
                content_stream
            )
        );
    }

    flags = read_multi_byte_value<uint16_t>(content_stream);
}

auto bytecode::ClassFile::access_flags() const -> uint16_t {
    return flags;
}

auto bytecode::ClassFile::constant_pool_entries() const -> std::span<const ConstantPoolEntry> {
    return constant_pool;
}

auto bytecode::ClassFile::major_version() const -> uint16_t {
    return version.major_version;
}

auto bytecode::ClassFile::minor_version() const -> uint16_t {
    return version.minor_version;
}
