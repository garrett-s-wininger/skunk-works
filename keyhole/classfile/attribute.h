#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <expected>

#include "reader.h"

namespace attribute {

struct Attribute {
    uint16_t name_index;
    std::span<const std::byte> data;

    static auto parse(reader::Reader&)
        -> std::expected<Attribute, reader::ParseError>;
};

}

#endif // ATTRIBUTE_H
