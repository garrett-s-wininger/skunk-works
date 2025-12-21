#include "parsing.h"

auto parsing::parse_attribute(reader::Reader& reader)
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
