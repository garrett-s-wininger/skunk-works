#include "parsing.h"

auto parsing::parse_attribute(reader::Reader& reader) noexcept
        -> std::expected<attribute::Attribute, parsing::Error> {
    const auto header = reader.read_bytes(sizeof(uint32_t) + sizeof(uint16_t));

    if (!header) {
        return std::unexpected(parsing::Error::Truncated);
    }

    reader::Reader header_reader{header.value()};

    const auto name_index = header_reader.read_unchecked<uint16_t>();
    const auto body_size = header_reader.read_unchecked<uint32_t>();

    const auto body = reader.read_bytes(body_size);

    if (!body) {
        return std::unexpected(parsing::Error::Truncated);
    }

    return attribute::Attribute{
        name_index,
        body.value()
    };
}

auto parsing::parse_method(reader::Reader& reader)
        -> std::expected<method::Method, parsing::Error> {
    const auto method_header = reader.read_bytes(sizeof(uint64_t));

    if (!method_header) {
        return std::unexpected(parsing::Error::Truncated);
    }

    reader::Reader method_reader{method_header.value()};

    const auto access_flags = method_reader.read_unchecked<uint16_t>();
    const auto name_index = method_reader.read_unchecked<uint16_t>();
    const auto descriptor_index = method_reader.read_unchecked<uint16_t>();
    const auto attribute_count = method_reader.read_unchecked<uint16_t>();

    std::vector<attribute::Attribute> attributes{};
    attributes.reserve(attribute_count);

    for (auto i = 0uz; i < attribute_count; ++i) {
        const auto result = parse_attribute(reader);

        if (!result) {
            return std::unexpected(parsing::Error::Truncated);
        }

        attributes.push_back(result.value());
    }

    return method::Method{
        access_flags,
        name_index,
        descriptor_index,
        std::move(attributes)
    };
}

auto parse_class_info_entry(reader::Reader& reader) noexcept
        -> std::expected<constant_pool::ClassEntry, parsing::Error> {
    const auto index = reader.read<uint16_t>();

    if (!index) {
        return std::unexpected(parsing::Error::Truncated);
    }

    return constant_pool::ClassEntry{index.value()};
}

// TODO(garrett): Perhaps collapse these (any maybe others) to a generic
// x-bytes parse
auto parse_method_reference_entry(reader::Reader& reader) noexcept
        -> std::expected<constant_pool::MethodReferenceEntry, parsing::Error> {
    const auto entry_contents = reader.read_bytes(sizeof(uint32_t));

    if (!entry_contents) {
        return std::unexpected(parsing::Error::Truncated);
    }

    reader::Reader entry_reader{entry_contents.value()};

    return constant_pool::MethodReferenceEntry{
        entry_reader.read_unchecked<uint16_t>(),
        entry_reader.read_unchecked<uint16_t>()
    };
}

auto parse_name_and_type_entry(reader::Reader& reader) noexcept
        -> std::expected<constant_pool::NameAndTypeEntry, parsing::Error> {
    const auto entry_contents = reader.read_bytes(sizeof(uint32_t));

    if (!entry_contents) {
        return std::unexpected(parsing::Error::Truncated);
    }

    reader::Reader entry_reader{entry_contents.value()};

    return constant_pool::NameAndTypeEntry{
        entry_reader.read_unchecked<uint16_t>(),
        entry_reader.read_unchecked<uint16_t>()
    };
}

auto parse_utf8_entry(reader::Reader& reader) noexcept
        -> std::expected<constant_pool::UTF8Entry, parsing::Error> {
    auto entry = constant_pool::UTF8Entry{};
    const auto size = reader.read<uint16_t>();

    if (!size) {
        return std::unexpected(parsing::Error::Truncated);
    }

    const auto text_content = reader.read_bytes(size.value());

    if (!text_content) {
        return std::unexpected(parsing::Error::Truncated);
    }

    entry.text = std::string_view{
        reinterpret_cast<const char*>(text_content.value().data()),
        size.value()
    };

    return entry;
}

auto parsing::parse_constant_pool_entry(reader::Reader& reader) noexcept
        -> std::expected<constant_pool::Entry, parsing::Error> {
    const auto tag = reader.read<uint8_t>();

    if (!tag) {
        return std::unexpected(parsing::Error::Truncated);
    }

    switch (static_cast<constant_pool::Tag>(tag.value())) {
        case constant_pool::Tag::Class: {
            return parse_class_info_entry(reader);
        }
        case constant_pool::Tag::MethodReference: {
            return parse_method_reference_entry(reader);
        }
        case constant_pool::Tag::NameAndType: {
            return parse_name_and_type_entry(reader);
        }
        case constant_pool::Tag::UTF8: {
            return parse_utf8_entry(reader);
        }
        default:
            return std::unexpected(parsing::Error::InvalidConstantPoolTag);
    }
}

auto parsing::parse_constant_pool(reader::Reader& reader, uint16_t count)
        -> std::expected<constant_pool::ConstantPool, parsing::Error> {
    constant_pool::ConstantPool pool{};

    for (auto i = 0uz; i < count; ++i) {
        const auto entry = parsing::parse_constant_pool_entry(reader);

        if (!entry) {
            return std::unexpected(parsing::Error::Truncated);
        }

        pool.add(entry.value());
    }

    return pool;
}

auto parsing::parse_class_file(reader::Reader& reader)
        -> std::expected<classfile::ClassFile, parsing::Error> {
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

    const auto pool = parsing::parse_constant_pool(
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

