#include "classfile.h"

classfile::ClassFile::ClassFile() noexcept
    : version{classfile::Version{55u, 0u}}
    , class_index{0u}
    , superclass_index{0u}
    // NOTE(garrett): Public members after here
    , constant_pool(constant_pool::ConstantPool{})
    , access_flags(0x0021)
    , methods(std::vector<method::Method>{})
    , attributes(std::vector<attribute::Attribute>{}) {}

auto classfile::ClassFile::name() const -> std::string_view {
    const auto& class_entry = constant_pool.resolve<constant_pool::ClassEntry>(
        class_index
    );

    return constant_pool.resolve<constant_pool::UTF8Entry>(
        class_entry.name_index
    ).text;
}

auto classfile::ClassFile::parse(reader::Reader& reader) noexcept
        -> std::expected<ClassFile, parsing::Error> {
    const auto header = reader.read_bytes(sizeof(uint64_t) + sizeof(uint16_t));

    if (!header) {
        return std::unexpected(parsing::Truncated);
    }

    auto header_reader = reader::Reader{header.value()};

    if (header_reader.read_unchecked<uint32_t>() != 0xCAFEBABE) {
        return std::unexpected(parsing::Error::InvalidMagic);
    }

    const auto minor = header_reader.read_unchecked<uint16_t>();
    const auto major = header_reader.read_unchecked<uint16_t>();

    auto result = classfile::ClassFile{};
    result.version = classfile::Version{major, minor};

    const auto pool = constant_pool::ConstantPool::parse(
        reader,
        // NOTE(garrett): Classfile contains the actual count, plus one
        header_reader.read_unchecked<uint16_t>() - 1
    );

    if (!pool) {
        return std::unexpected(parsing::Error::Truncated);
    }

    result.constant_pool = pool.value();

    const auto access_and_class_metadata = reader.read_bytes(sizeof(uint64_t));

    if (!access_and_class_metadata) {
        return std::unexpected(parsing::Error::Truncated);
    }

    auto metadata_reader = reader::Reader{access_and_class_metadata.value()};
    result.access_flags = metadata_reader.read_unchecked<uint16_t>();
    result.class_index = metadata_reader.read_unchecked<uint16_t>();
    result.superclass_index = metadata_reader.read_unchecked<uint16_t>();

    const auto interface_count = metadata_reader.read_unchecked<uint16_t>();

    // TODO(garrett): Interface parsing
    if (interface_count > 0) {
        return std::unexpected(parsing::Error::NotImplemented);
    }

    const auto fields_count = reader.read<uint16_t>();

    // TODO(garrett): Field parsing
    if (!fields_count) {
        return std::unexpected(parsing::Error::Truncated);
    } else if (fields_count.value() > 0) {
        return std::unexpected(parsing::Error::NotImplemented);
    }

    const auto methods_count = reader.read<uint16_t>();

    if (!methods_count) {
        return std::unexpected(parsing::Error::Truncated);
    }

    result.methods = std::vector<method::Method>{};
    result.methods.reserve(methods_count.value());

    for (auto i = 0u; i < methods_count.value(); ++i) {
        const auto method = parsing::parse_method(reader);

        if (!method) {
            return std::unexpected(parsing::Error::Truncated);
        }

        result.methods.push_back(method.value());
    }

    const auto attributes_count = reader.read<uint16_t>();

    if (!attributes_count) {
        return std::unexpected(parsing::Error::Truncated);
    }

    result.attributes = std::vector<attribute::Attribute>{};

    for (auto i = 0uz; i < attributes_count.value(); ++i) {
        const auto attribute = parsing::parse_attribute(reader);

        if (!attribute) {
            return std::unexpected(parsing::Error::Truncated);
        }

        result.attributes.push_back(attribute.value());
    }

    return result;
}

auto classfile::ClassFile::superclass() const -> std::string_view {
    const auto& class_entry = constant_pool.resolve<constant_pool::ClassEntry>(
        superclass_index
    );

    return constant_pool.resolve<constant_pool::UTF8Entry>(
        class_entry.name_index
    ).text;
}
