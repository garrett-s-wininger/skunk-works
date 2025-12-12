#ifndef CLASSFILE_H
#define CLASSFILE_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

#include "endian.h"

namespace classfile {

template <typename S>
concept Sink = requires(S& sink, uint16_t u16, uint32_t u32) {
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

class ClassFile {
private:
    uint16_t major_version_;
    uint16_t minor_version_;
public:
    ClassFile() noexcept;

    auto dump_contents(Sink auto& sink) const -> void {
        sink.write(static_cast<uint32_t>(0xCAFEBABE));
        sink.write(static_cast<uint16_t>(minor_version_));
        sink.write(static_cast<uint16_t>(major_version_));

        // TODO(garrett): Write appropriate constant pool entries
        sink.write(static_cast<uint16_t>(0x0001));

        // TODO(garrett): Access flag support, default to ACC_SUPER | ACC_PUBLIC
        sink.write(static_cast<uint16_t>(0x0021));

        // TODO(garrett): Valid constant pool for class name
        sink.write(static_cast<uint16_t>(0x0000));

        // TODO(garrett): Valid constant pool for super class name
        sink.write(static_cast<uint16_t>(0x0000));

        // TODO(garrett): Write interface entries
        sink.write(static_cast<uint16_t>(0x0000));

        // TODO(garrett): Write field entries
        sink.write(static_cast<uint16_t>(0x0000));

        // TODO(garrett): Write method entries
        sink.write(static_cast<uint16_t>(0x0000));

        // TODO(garrett): Write attribute entries
        sink.write(static_cast<uint16_t>(0x0000));
    }

    auto major_version() const noexcept -> uint16_t;
    auto minor_version() const noexcept -> uint16_t;
};

}

#endif // CLASSFILE_H
