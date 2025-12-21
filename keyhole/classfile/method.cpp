#include "method.h"

auto method::Method::parse(reader::Reader& reader)
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
        const auto result = attribute::Attribute::parse(reader);

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
