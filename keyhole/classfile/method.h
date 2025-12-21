#ifndef METHOD_H
#define METHOD_H

#include <vector>

#include "attribute.h"
#include "parsing.h"
#include "reader.h"

namespace method {

enum class AccessFlags : uint16_t {
    ACC_PUBLIC = 0x0001,
    ACC_PRIVATE = 0x0002,
    ACC_PROTECTED = 0x0004,
    ACC_STATIC = 0x0008,
    ACC_FINAL = 0x0010,
    ACC_SYNCHRONIZED = 0x0020,
    ACC_BRIDGE = 0x0040,
    ACC_VARAGS = 0x0080,
    ACC_NATIVE = 0x0100,
    ACC_ABSTRACT = 0x0400,
    ACC_STRICT = 0x0800,
    ACC_SYNTHETIC = 0x1000
};

struct Method {
    uint16_t access_flags;
    uint16_t name_index;
    uint16_t descriptor_index;
    std::vector<attribute::Attribute> attributes;

    static auto parse(reader::Reader&)
        -> std::expected<Method, parsing::Error>;
};

}

#endif // METHOD_H
