#ifndef PARSING_H
#define PARSING_H

#include <expected>

#include "attribute.h"
#include "method.h"
#include "reader.h"

namespace parsing {

enum Error {
    InvalidConstantPoolTag,
    InvalidMagic,
    NotImplemented,
    Truncated
};

auto parse_attribute(reader::Reader&) -> std::expected<attribute::Attribute, Error>;
auto parse_method(reader::Reader&) -> std::expected<method::Method, Error>;

}

#endif // PARSING_H
