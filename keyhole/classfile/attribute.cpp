#include "attribute.h"

auto attribute::Attribute::parse(reader::Reader& reader)
        -> std::expected<attribute::Attribute, reader::Error> {
    const auto header = reader.read_bytes(sizeof(uint32_t) + sizeof(uint16_t));

    if (!header) {
        return std::unexpected(header.error());
    }

    reader::Reader header_reader{header.value()};

    const auto name_index = header_reader.read_unchecked<uint16_t>();
    const auto body_size = header_reader.read_unchecked<uint32_t>();

    const auto body = reader.read_bytes(body_size);

    if (!body) {
        return std::unexpected(header.error());
    }

    return attribute::Attribute{
        name_index,
        body.value()
    };
}
