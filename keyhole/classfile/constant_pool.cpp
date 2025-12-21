#include "constant_pool.h"

constant_pool::ConstantPool::ConstantPool()
        : entries_(std::deque<constant_pool::Entry>{})
        , resolution_table_(std::vector<std::optional<size_t>>{}) {
    // NOTE(garrett): Index zero is reserved, access should be 1-indexed so we
    // can grab data directly from other classfile references
    resolution_table_.push_back(std::nullopt);
}

constant_pool::ConstantPool::ConstantPool(std::initializer_list<constant_pool::Entry> entries)
        : constant_pool::ConstantPool::ConstantPool() {
    for (const auto& entry : entries) {
        add(entry);
    }
}

auto constant_pool::ConstantPool::add(const Entry entry) -> void {
    resolution_table_.push_back(entries_.size());
    entries_.push_back(entry);
}

auto constant_pool::ConstantPool::entries() const -> const std::deque<Entry>& {
    return entries_;
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

auto constant_pool::ConstantPool::parse_entry(reader::Reader& reader) noexcept
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

auto constant_pool::ConstantPool::parse(reader::Reader& reader, uint16_t count)
        noexcept -> std::expected<constant_pool::ConstantPool, parsing::Error> {
    constant_pool::ConstantPool pool{};

    for (auto i = 0uz; i < count; ++i) {
        const auto entry = constant_pool::ConstantPool::parse_entry(reader);

        if (!entry) {
            return std::unexpected(parsing::Error::Truncated);
        }

        pool.add(entry.value());
    }

    return pool;
}
