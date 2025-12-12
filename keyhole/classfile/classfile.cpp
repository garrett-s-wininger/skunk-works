#include "classfile.h"

classfile::VectorSink::VectorSink() : buffer_(std::vector<std::byte>()) {}

classfile::VectorSink::VectorSink(std::vector<std::byte>& buffer) noexcept
    : buffer_(buffer) {}

auto classfile::VectorSink::view() const noexcept -> std::span<const std::byte> {
    return buffer_;
}

classfile::ClassFile::ClassFile() noexcept
    : major_version_(55u)
    , minor_version_(0u) {}

auto classfile::ClassFile::major_version() const noexcept -> uint16_t {
    return major_version_;
}

auto classfile::ClassFile::minor_version() const noexcept -> uint16_t {
    return minor_version_;
}
