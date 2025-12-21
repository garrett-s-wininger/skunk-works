#include "gtest/gtest.h"

#include "parsing.h"

TEST(Parsing, ParsesMethod) {
    constexpr auto input = std::array<const std::byte, 15>{
        // Access
        std::byte{0x00}, std::byte{0x11},
        // Name index
        std::byte{0x00}, std::byte{0x01},
        // Descriptor index
        std::byte{0x00}, std::byte{0x02},
        // Attribute count
        std::byte{0x00}, std::byte{0x01},
        // Attribute name index
        std::byte{0x00}, std::byte{0x0A},
        // Attribute length
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x01},
        // Attribute data
        std::byte{'Z'}
    };

    reader::Reader reader{input};

    const auto result = parsing::parse_method(reader);

    ASSERT_TRUE(result);
    const auto method = result.value();

    ASSERT_EQ(
        static_cast<uint16_t>(method::AccessFlags::ACC_PUBLIC)
        | static_cast<uint16_t>(method::AccessFlags::ACC_FINAL),
        method.access_flags
    );

    ASSERT_EQ(1u, method.name_index);
    ASSERT_EQ(2u, method.descriptor_index);
    ASSERT_EQ(1u, method.attributes.size());
}
