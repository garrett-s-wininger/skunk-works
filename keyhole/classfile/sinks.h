#ifndef SINKS_H
#define SINKS_H

#include <fstream>
#include <span>
#include <vector>

#include "endian.h"

namespace sinks {

template <typename S>
concept Sink = requires(S& sink, uint8_t u8, uint16_t u16, uint32_t u32) {
    { sink.write(u8) } -> std::same_as<void>;
    { sink.write(u16) } -> std::same_as<void>;
    { sink.write(u32) } -> std::same_as<void>;
};

class FileSink {
private:
    std::ofstream& target_;
public:
    FileSink(std::ofstream&);

    template <endian::MultiByteIntegral V>
    auto write(V value) -> void {
        auto bytes = std::bit_cast<std::array<std::byte, sizeof(V)>>(
            endian::big(value)
        );

        target_.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
    }
};

class VectorSink {
private:
    std::vector<std::byte> buffer_;
public:
    VectorSink();
    VectorSink(std::vector<std::byte>&) noexcept;

    template <endian::MultiByteIntegral V>
    auto write(V value) -> void {
        auto bytes = std::bit_cast<std::array<std::byte, sizeof(V)>>(
            endian::big(value)
        );

        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    auto view() const noexcept -> std::span<const std::byte>;
};

}

#endif // SINKS_H
