#ifndef READER_H
#define READER_H

#include <algorithm>
#include <expected>
#include <span>

#include "endian.h"

namespace reader {

enum Error {
    Truncated,
};

class Reader {
private:
    std::span<const std::byte> remaining_;
public:
    explicit Reader(std::span<const std::byte>) noexcept;

    auto read_bytes(uint32_t)
        -> std::expected<std::span<const std::byte>, Error>;

    template <endian::MultiByteIntegral V>
    auto read_unchecked() -> V {
        std::array<std::byte, sizeof(V)> buffer;
        std::copy_n(remaining_.begin(), sizeof(V), buffer.begin());

        remaining_ = remaining_.subspan(sizeof(V));
        return endian::big(std::bit_cast<V>(buffer));
    }

    template <endian::MultiByteIntegral V>
    auto read() -> std::expected<V, Error> {
        if (remaining_.size() < sizeof(V)) {
            return std::unexpected(Error::Truncated);
        }

        return read_unchecked<V>();
    }
};

}

#endif // READER_H
