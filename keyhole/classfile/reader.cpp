#include "reader.h"

reader::Reader::Reader(std::span<const std::byte> bytes) noexcept
    : remaining_(bytes) {}

auto reader::Reader::read_bytes(uint32_t count)
        -> std::expected<std::span<const std::byte>, reader::ParseError> {
    if (remaining_.size() < count) {
        return std::unexpected(reader::ParseError::Truncated);
    }

    const auto result = remaining_.first(count);
    remaining_ = remaining_.subspan(count);

    return result;
}
