#include "gtest/gtest.h"

#include "parsing.h"

using namespace std::literals;

TEST(Parsing, ParsesAttribute) {
    constexpr auto input = std::array<const std::byte, 7> {
        // Name index
        std::byte{0x00}, std::byte{0x01},
        // Attribute length,
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x01},
        // Content
        std::byte{'A'}
    };

    reader::Reader reader{input};
    const auto result = parsing::parse_attribute(reader);

    ASSERT_TRUE(result);

    const auto attribute = result.value();

    ASSERT_EQ(1, attribute.name_index);
    ASSERT_EQ(1, attribute.data.size());
    ASSERT_EQ(std::byte{'A'}, attribute.data[0]);
}

TEST(Parsing, DetectsInvalidConstantPoolTag) {
    constexpr auto input = std::array<std::byte, 1>{
        std::byte{0xFF}
    };

    reader::Reader reader{input};
    const auto result = parsing::parse_constant_pool_entry(reader);

    ASSERT_FALSE(result);

    ASSERT_EQ(
        parsing::Error::InvalidConstantPoolTag,
        result.error()
    );
}

TEST(Parsing, ParsesClassEntry) {
    constexpr auto input = std::array<std::byte, 3>{
        // Tag
        std::byte{0x07},
        // Index
        std::byte{0x00}, std::byte{0x01}
    };

    reader::Reader reader{input};
    const auto result = parsing::parse_constant_pool_entry(reader);

    ASSERT_TRUE(result);
    ASSERT_TRUE(
        std::holds_alternative<constant_pool::ClassEntry>(result.value())
    );

    const auto entry = std::get<constant_pool::ClassEntry>(result.value());
    ASSERT_EQ(1, entry.name_index);
}

TEST(Parsing, ParsesMethodReferenceEntry) {
    constexpr auto input = std::array<std::byte, 5>{
        // Tag
        std::byte{0x0A},
        // Class index
        std::byte{0x00}, std::byte{0x01},
        // Descriptor index
        std::byte{0x00}, std::byte{0x02}
    };


    reader::Reader reader{input};
    const auto result = parsing::parse_constant_pool_entry(reader);

    ASSERT_TRUE(result);
    ASSERT_TRUE(
        std::holds_alternative<constant_pool::MethodReferenceEntry>(result.value())
    );

    const auto entry = std::get<constant_pool::MethodReferenceEntry>(result.value());
    ASSERT_EQ(1, entry.class_index);
    ASSERT_EQ(2, entry.name_and_type_index);
}

TEST(Parsing, ParsesNameAndTypeEntry) {
    constexpr auto input = std::array<std::byte, 5>{
        // Tag
        std::byte{0x0C},
        // Name index
        std::byte{0x00}, std::byte{0x02},
        // Descriptor index
        std::byte{0x00}, std::byte{0x04}
    };

    reader::Reader reader{input};
    const auto result = parsing::parse_constant_pool_entry(reader);

    ASSERT_TRUE(result);
    ASSERT_TRUE(
        std::holds_alternative<constant_pool::NameAndTypeEntry>(result.value())
    );

    const auto entry = std::get<constant_pool::NameAndTypeEntry>(result.value());
    ASSERT_EQ(2, entry.name_index);
    ASSERT_EQ(4, entry.descriptor_index);
}

TEST(Parsing, ParsesUTF8Entry) {
    constexpr auto input = std::array<std::byte, 8>{
        // Tag
        std::byte{0x01},
        // Length
        std::byte{0x00}, std::byte{0x05},
        // Content
        std::byte{'C'}, std::byte{'l'}, std::byte{'a'},
        std::byte{'s'}, std::byte{'s'}
    };

    reader::Reader reader{input};
    const auto result = parsing::parse_constant_pool_entry(reader);

    ASSERT_TRUE(result);
    ASSERT_TRUE(
        std::holds_alternative<constant_pool::UTF8Entry>(result.value())
    );

    const auto entry = std::get<constant_pool::UTF8Entry>(result.value());

    ASSERT_EQ(5, entry.text.size());
    ASSERT_EQ("Class"sv, entry.text);
}

TEST(Parsing, ParsesConstantPool) {
    constexpr auto input = std::array<std::byte, 7>{
        // UTF8 entry
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x01},
        std::byte{'A'},
        // Class entry
        std::byte{0x07},
        std::byte{0x00}, std::byte{0x01}
    };

    reader::Reader reader{input};
    const auto pool_parse_result = parsing::parse_constant_pool(reader, 2);

    ASSERT_TRUE(pool_parse_result);
    ASSERT_EQ(2uz, pool_parse_result.value().entries().size());
}

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
