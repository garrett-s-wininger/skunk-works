#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <cstdint>
#include <span>

namespace attribute {

struct Attribute {
    uint16_t name_index;
    std::span<const std::byte> data;
};

}

#endif // ATTRIBUTE_H
