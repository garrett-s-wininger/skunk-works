#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <expected>
#include <vector>

#include "reader.h"

namespace attribute {

struct Attribute {
    uint16_t name_index;
    std::vector<std::byte> data;

    static auto parse(reader::Reader&)
        -> std::expected<Attribute, reader::ParseError>;
};

}

#endif
