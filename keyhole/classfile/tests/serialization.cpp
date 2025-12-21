#include "gtest/gtest.h"

#include "serialization.h"
#include "tests/helpers.h"

TEST(Serialization, SerializesAttribute) {
    std::vector<std::byte> data{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}
    };

    attribute::Attribute attribute{
        12u,
        data
    };

    sinks::VectorSink sink{};
    serialization::serialize(sink, attribute);

    constexpr auto expected = std::array<std::byte, 9>{
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
