#include <format>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "classfile.h"

MATCHER_P(EqualsBinary, expected, "Binary elements are equal in size and value") {
    if (arg.size() != expected.size()) {
        *result_listener << "Size of spans did not match: "
            << expected.size() << " expected, " << arg.size() << " actual";

        return false;
    }

    for (auto i = 0uz; i < expected.size(); ++i) {
        if (expected[i] != arg[i]) {
            *result_listener << "Encountered mismatch at byte index " << i
                << ", expected " 
                << std::format("0x{:02X}", std::to_integer<unsigned>(expected[i]))
                << " but got "
                << std::format("0x{:02X}", std::to_integer<unsigned>(arg[i]));

            return false;
        }
    }

    return true;
}

TEST(ClassFile, DefaultVersionAtJava11) {
    classfile::ClassFile klass{};

    EXPECT_EQ(55u, klass.major_version());
    EXPECT_EQ(0u, klass.minor_version());
}

TEST(ClassFile, OutputsCorrectBinaryData) {
    classfile::ClassFile klass{};
    classfile::VectorSink sink{};
    klass.dump_contents(sink);

    auto expected = std::array<const std::byte, 24> {
        // NOTE(garrett): All multi-byte values in Big-Endian representation
        // Magic - u32
        std::byte{0xCA}, std::byte{0xFE}, std::byte{0xBA}, std::byte{0xBE},
        // Minor - u16
        std::byte{0x00}, std::byte{0x00},
        // Major - u16
        std::byte{0x00}, std::byte{0x37},
        // Constant Pool count + 1
        std::byte{0x00}, std::byte{0x01},
        // Access flags
        std::byte{0x00}, std::byte{0x21},
        // Class name index
        std::byte{0x00}, std::byte{0x00},
        // Super class name index
        std::byte{0x00}, std::byte{0x00},
        // Inteface count
        std::byte{0x00}, std::byte{0x00},
        // Field count
        std::byte{0x00}, std::byte{0x00},
        // Mehod count
        std::byte{0x00}, std::byte{0x00},
        // Attribute count
        std::byte{0x00}, std::byte{0x00},
    };

    auto actual = sink.view();
    EXPECT_THAT(expected, EqualsBinary(actual));
}
