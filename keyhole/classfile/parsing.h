#ifndef PARSING_H
#define PARSING_H

#include <expected>

#include "method.h"
#include "reader.h"

namespace parsing {

enum Error {
    InvalidConstantPoolTag,
    InvalidMagic,
    NotImplemented,
    Truncated
};

auto parse_method(reader::Reader&) -> std::expected<method::Method, Error>;

}

#endif // PARSING_H
