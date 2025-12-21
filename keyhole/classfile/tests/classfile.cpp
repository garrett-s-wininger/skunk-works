#include "gtest/gtest.h"

#include "classfile.h"
#include "sinks.h"
#include "tests/helpers.h"

TEST(ClassFile, DetectsInvalidMagic) {
    const auto input = std::to_array({
        std::byte{0xBE}, std::byte{0xBA}, std::byte{0xFE}, std::byte{0xCA},
        // NOTE(garrett): Remaining bytes just padding to handle bulk read
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}
    });

    reader::Reader reader{input};
    const auto result = classfile::ClassFile::parse(reader);

    ASSERT_FALSE(result);
    EXPECT_EQ(parsing::Error::InvalidMagic, result.error());
}

TEST(ClassFile, DetectsTruncation) {
    const auto input = std::array<const std::byte, 0>{};

    reader::Reader reader{input};
    const auto result = classfile::ClassFile::parse(reader);

    ASSERT_FALSE(result);
    EXPECT_EQ(parsing::Error::Truncated, result.error());
}

TEST(ClassFile, ParsesAppropriately) {
    constexpr auto input = std::to_array<const std::byte>({
        // NOTE(garrett): All multi-byte values in Big-Endian representation
        // Magic - u32
        std::byte{0xCA}, std::byte{0xFE}, std::byte{0xBA}, std::byte{0xBE},
        // Minor - u16
        std::byte{0x00}, std::byte{0x00},
        // Major - u16
        std::byte{0x00}, std::byte{0x3D},
        // Constant pool count + 1
        std::byte{0x00}, std::byte{0x08},
        // CP #1 - UTF8
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x01}, std::byte{'A'},
        // CP #2 - Class
        std::byte{0x07},
        std::byte{0x00}, std::byte{0x01},
        // CP #3 - UTF8
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x10},
        std::byte{'j'}, std::byte{'a'}, std::byte{'v'}, std::byte{'a'},
        std::byte{'/'}, std::byte{'l'}, std::byte{'a'}, std::byte{'n'},
        std::byte{'g'}, std::byte{'/'}, std::byte{'O'}, std::byte{'b'},
        std::byte{'j'}, std::byte{'e'}, std::byte{'c'}, std::byte{'t'},
        // CP #4 - Class
        std::byte{0x07},
        std::byte{0x00}, std::byte{0x03},
        // CP #5 - UTF8
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x06},
        std::byte{'<'}, std::byte{'i'}, std::byte{'n'}, std::byte{'i'},
        std::byte{'t'}, std::byte{'>'},
        // CP #6 - UTF8
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x03},
        std::byte{'('}, std::byte{')'}, std::byte{'V'},
        // CP #7 - UTF8
        std::byte{0x01},
        std::byte{0x00}, std::byte{0x0A},
        std::byte{'D'}, std::byte{'e'}, std::byte{'p'}, std::byte{'r'},
        std::byte{'e'}, std::byte{'c'}, std::byte{'a'}, std::byte{'t'},
        std::byte{'e'}, std::byte{'d'},
        // Access flags
        std::byte{0x00}, std::byte{0x31},
        // Class index
        std::byte{0x00}, std::byte{0x02},
        // Superclass index
        std::byte{0x00}, std::byte{0x04},
        // Interface count
        std::byte{0x00}, std::byte{0x00},
        // Field count
        std::byte{0x00}, std::byte{0x00},
        // Method count
        std::byte{0x00}, std::byte{0x01},
        // Method access flags
        std::byte{0x00}, std::byte{0x01},
        // Method name index
        std::byte{0x00}, std::byte{0x05},
        // Method descriptor index
        std::byte{0x00}, std::byte{0x06},
        // Method attribute count
        std::byte{0x00}, std::byte{0x00},
        // Attribute count
        std::byte{0x00}, std::byte{0x01},
        // Attribute name index
        std::byte{0x00}, std::byte{0x07},
        // Attribute length
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}
    });

    reader::Reader reader{input};
    const auto result = classfile::ClassFile::parse(reader);

    ASSERT_TRUE(result);

    const auto klass = result.value();

    ASSERT_EQ(61u, klass.version.major);
    ASSERT_EQ(0u, klass.version.minor);

    const auto parsed_constant_pool = klass.constant_pool;

    ASSERT_EQ(7u, parsed_constant_pool.entries().size());

    {
        using classfile::AccessFlags;

        ASSERT_EQ(
            static_cast<uint16_t>(AccessFlags::ACC_FINAL)
            | static_cast<uint16_t>(AccessFlags::ACC_PUBLIC)
            | static_cast<uint16_t>(AccessFlags::ACC_SUPER),
            klass.access_flags
        );
    }

    ASSERT_EQ(1u, klass.methods.size());
    ASSERT_EQ(1u, klass.attributes.size());
}

TEST(ClassFile, OutputsCorrectBinaryData) {
    const std::string class_name{"MyClass"};
    const std::string superclass_name{"java/lang/Object"};
    const classfile::ClassFile klass(class_name, superclass_name);

    sinks::VectorSink sink{};
    klass.dump_contents(sink);

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
