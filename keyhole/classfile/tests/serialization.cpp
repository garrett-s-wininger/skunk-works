#include "gtest/gtest.h"

#include "serialization.h"
#include "tests/helpers.h"

TEST(Serialization, SerializesAttribute) {
    constexpr std::array<const std::byte, 3> data{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}
    };

    attribute::Attribute attribute{
        12u,
        data
    };

    sinks::VectorSink sink{};
    serialization::serialize(sink, attribute);

    constexpr std::array<std::byte, 9> expected{
        // Name index
        std::byte{0x00}, std::byte{0x0C},
        // Length
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x03},
        // Data
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}
    };

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}

TEST(Serialization, SerializesMethod) {
    method::Method method{
        static_cast<uint16_t>(method::AccessFlags::ACC_PUBLIC),
        3u,
        4u,
        std::vector<attribute::Attribute>{
            attribute::Attribute{
                5u,
                std::span<const std::byte>{}
            }
        }
    };

    sinks::VectorSink sink{};
    serialization::serialize(sink, method);

    constexpr auto expected = std::to_array({
        // Method access
        std::byte{0x00}, std::byte{0x01},
        // Name index
        std::byte{0x00}, std::byte{0x03},
        // Descriptor index
        std::byte{0x00}, std::byte{0x04},
        // Attributes, 1 (empty)
        std::byte{0x00}, std::byte{0x05},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}
    });

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}
