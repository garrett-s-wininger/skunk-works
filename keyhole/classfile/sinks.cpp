#include "sinks.h"

sinks::FileSink::FileSink(std::ofstream& target) : target_(target) {}

sinks::VectorSink::VectorSink() : buffer_(std::vector<std::byte>()) {}

sinks::VectorSink::VectorSink(std::vector<std::byte>& buffer) noexcept
    : buffer_(buffer) {}

auto sinks::VectorSink::view() const noexcept -> std::span<const std::byte> {
    return buffer_;
}


