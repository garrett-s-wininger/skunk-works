#include "gtest/gtest.h"

#include "serialization.h"
#include "tests/helpers.h"

TEST(Serialization, SerializesAttribute) {
    constexpr std::array<const std::byte, 3> data{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}
    };

    const attribute::Attribute attribute{
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
    const method::Method method{
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

TEST(Serialization, SerializesClassEntries) {
    constexpr constant_pool::ClassEntry entry{16};
    sinks::VectorSink sink{};

    serialization::serialize(sink, entry);

    constexpr auto expected = std::array<const std::byte, 3>{
        std::byte{0x07}, std::byte{0x00}, std::byte{0x10}
    };

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}

TEST(Serialization, SerializesConstantPool) {
    const std::string text = "A";

    const constant_pool::ConstantPool pool{
        constant_pool::ClassEntry{2u},
        constant_pool::UTF8Entry{text}
    };

    sinks::VectorSink sink{};

    serialization::serialize(sink, pool);

    constexpr auto expected = std::array<const std::byte, 7>{
        // Class entry
        std::byte{0x07},
        std::byte{0x00}, std::byte{0x02},
        // UTF8 entry
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x01},
        std::byte{'A'}
    };

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}

TEST(Serialization, SerializesMethodReferenceEntries) {
    constexpr constant_pool::MethodReferenceEntry entry{1, 2};
    sinks::VectorSink sink{};

    serialization::serialize(sink, entry);

    constexpr auto expected = std::array<const std::byte, 5>{
        // Tag
        std::byte{0x0A},
        // Class index
        std::byte{0x00}, std::byte{0x01},
        // Descriptor index
        std::byte{0x00}, std::byte{0x02}
    };

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}

TEST(Serialization, SerializesNameAndTypeEntries) {
    constexpr constant_pool::NameAndTypeEntry entry{2, 4};
    sinks::VectorSink sink{};

    serialization::serialize(sink, entry);

    constexpr auto expected = std::array<const std::byte, 5>{
        // Tag
        std::byte{0x0C},
        // Name index
        std::byte{0x00}, std::byte{0x02},
        // Descriptor index
        std::byte{0x00}, std::byte{0x04}
    };

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}

TEST(Serialization, SerializesUTF8Entries) {
    const constant_pool::UTF8Entry entry{"MyClass"};
    sinks::VectorSink sink{};

    serialization::serialize(sink, entry);

    constexpr auto expected = std::array<const std::byte, 10>{
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x07},
        std::byte{'M'}, std::byte{'y'}, std::byte{'C'}, std::byte{'l'},
        std::byte{'a'}, std::byte{'s'}, std::byte{'s'}
    };

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}

TEST(Serialization, SerializesClassFile) {
    const std::string class_name{"MyClass"};
    const std::string superclass_name{"java/lang/Object"};
    const classfile::ClassFile klass(class_name, superclass_name);

    sinks::VectorSink sink{};
    serialization::serialize(sink, klass);

    constexpr auto expected = std::to_array<const std::byte>({
        // NOTE(garrett): All multi-byte values in Big-Endian representation
        // Magic - u32
        std::byte{0xCA}, std::byte{0xFE}, std::byte{0xBA}, std::byte{0xBE},
        // Minor - u16
        std::byte{0x00}, std::byte{0x00},
        // Major - u16
        std::byte{0x00}, std::byte{0x37},
        // Constant Pool count + 1
        std::byte{0x00}, std::byte{0x05},
        // Name UTF8 Entry
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x07},
        std::byte{'M'}, std::byte{'y'}, std::byte{'C'}, std::byte{'l'},
        std::byte{'a'}, std::byte{'s'}, std::byte{'s'},
        // Class info entry
        std::byte{0x07}, std::byte{0x00}, std::byte{0x01},
        // Name UTF8 Entry
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x10},
        std::byte{'j'}, std::byte{'a'}, std::byte{'v'}, std::byte{'a'},
        std::byte{'/'}, std::byte{'l'}, std::byte{'a'}, std::byte{'n'},
        std::byte{'g'}, std::byte{'/'}, std::byte{'O'}, std::byte{'b'},
        std::byte{'j'}, std::byte{'e'}, std::byte{'c'}, std::byte{'t'},
        // Class info entry
        std::byte{0x07}, std::byte{0x00}, std::byte{0x03},
        // Access flags
        std::byte{0x00}, std::byte{0x21},
        // Class name index
        std::byte{0x00}, std::byte{0x02},
        // Super class name index
        std::byte{0x00}, std::byte{0x04},
        // Interface count
        std::byte{0x00}, std::byte{0x00},
        // Field count
        std::byte{0x00}, std::byte{0x00},
        // Method count
        std::byte{0x00}, std::byte{0x00},
        // Attribute count
        std::byte{0x00}, std::byte{0x00},
    });

    const auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}
