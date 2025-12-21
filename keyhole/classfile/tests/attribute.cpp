#include "gtest/gtest.h"

#include "attribute.h"

TEST(Attribute, ParsesAppropriately) {
    constexpr auto input = std::array<const std::byte, 7> {
        // Name index
        std::byte{0x00}, std::byte{0x01},
        // Attribute length,
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x01},
        // Content
        std::byte{'A'}
    };

    reader::Reader reader{input};
    const auto result = attribute::Attribute::parse(reader);

    ASSERT_TRUE(result);

    const auto attribute = result.value();

    ASSERT_EQ(1, attribute.name_index);
    ASSERT_EQ(1, attribute.data.size());
    ASSERT_EQ(std::byte{'A'}, attribute.data[0]);
}
