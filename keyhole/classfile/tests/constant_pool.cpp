#include "gtest/gtest.h"

#include "constant_pool.h"
#include "tests/helpers.h"

using namespace std::literals;

TEST(ConstantPool, GeneratesCorrectTagValues) {
    constexpr constant_pool::ClassEntry klass{};
    constexpr constant_pool::MethodReferenceEntry method_ref{};
    constexpr constant_pool::NameAndTypeEntry name_and_type{};
    const constant_pool::UTF8Entry utf8{};

    EXPECT_EQ(7, static_cast<uint8_t>(constant_pool::tag(klass)));
    EXPECT_EQ(10, static_cast<uint8_t>(constant_pool::tag(method_ref)));
    EXPECT_EQ(12, static_cast<uint8_t>(constant_pool::tag(name_and_type)));
    EXPECT_EQ(1, static_cast<uint8_t>(constant_pool::tag(utf8)));
}

TEST(ConstantPool, ResolveFailsOnTypeMismatch) {
    const constant_pool::ConstantPool pool{
        constant_pool::ClassEntry{1u}
    };

    ASSERT_ANY_THROW(pool.resolve<constant_pool::UTF8Entry>(1u));
}

TEST(ConstantPool, ResolveFailsOnOutOfBoundsIndex) {
    const constant_pool::ConstantPool pool{};
    ASSERT_ANY_THROW(pool.resolve<constant_pool::UTF8Entry>(15u));
}

TEST(ConstantPool, ResolveFailesOnZeroIndex) {
    const constant_pool::ConstantPool pool{
        constant_pool::UTF8Entry{std::string("Test")}
    };

    ASSERT_ANY_THROW(pool.resolve<constant_pool::UTF8Entry>(0u));
}

TEST(ConstantPool, ResolveProperlyGrabsEntryReference) {
    const constant_pool::ConstantPool pool{
        constant_pool::UTF8Entry{std::string("ExampleEntry")}
    };

    const auto& entry = pool.resolve<constant_pool::UTF8Entry>(1u);
    const auto& entry2 = pool.resolve<constant_pool::UTF8Entry>(1u);

    ASSERT_EQ(std::addressof(entry), std::addressof(entry2));
}
