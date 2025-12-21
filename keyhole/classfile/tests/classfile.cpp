#include "gtest/gtest.h"

#include "classfile.h"
#include "sinks.h"
#include "tests/helpers.h"

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
