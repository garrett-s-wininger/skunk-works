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
    class_name_index = read_multi_byte_value<uint16_t>(content_stream);
    super_class_name_index = read_multi_byte_value<uint16_t>(content_stream);
}

auto bytecode::ClassFile::access_flags() const -> uint16_t {
    return flags;
}

// NOTE(garrett): This function could generally just get removed, but, the
// long and double constant pool entries require an additional index
// bump, per the JVMLS
auto bytecode::ClassFile::get_constant_pool_entry(uint16_t requested) const -> const ConstantPoolEntry& {
    auto index = 1uz;

    for (auto i = 0uz; i < constant_pool.size(); ++i, ++index) {
        if (index == requested) {
            return constant_pool[i];
        }
    }

    throw std::runtime_error(
        std::format("Constant pool index {} out of range", index)
    );
}

auto bytecode::ClassFile::get_name_of_class_entry(const ConstantPoolClass entry) const -> std::string {
    const auto name_entry = get_constant_pool_entry(entry.name_index);

    if (!std::holds_alternative<ConstantPoolUTF8>(name_entry)) {
        throw std::runtime_error(
            "Constant pool class entry for class name points to non-UTF8 entry"
        );
    }

    return std::get<ConstantPoolUTF8>(name_entry).text;
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

auto bytecode::ClassFile::name() const -> std::string {
    const auto class_entry = get_constant_pool_entry(class_name_index);

    if (!std::holds_alternative<ConstantPoolClass>(class_entry)) {
        throw std::runtime_error(
            "Constant pool entry for class name points to non-class entry"
        );
    }

    return get_name_of_class_entry(std::get<ConstantPoolClass>(class_entry));
}

auto bytecode::ClassFile::super_class() const -> std::string {
    const auto class_entry = get_constant_pool_entry(super_class_name_index);

    if (!std::holds_alternative<ConstantPoolClass>(class_entry)) {
        throw std::runtime_error(
            "Constant pool entry for super class name points to non-class entry"
        );
    }

    return get_name_of_class_entry(std::get<ConstantPoolClass>(class_entry));
}
