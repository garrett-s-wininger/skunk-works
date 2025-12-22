#ifndef CLASSFILE_H
#define CLASSFILE_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include "attribute.h"
#include "constant_pool.h"
#include "method.h"

namespace classfile {

struct Version {
    uint16_t major;
    uint16_t minor;
};

enum class AccessFlags : uint16_t {
    ACC_PUBLIC = 0x0001,
    ACC_FINAL = 0x0010,
    ACC_SUPER = 0x0020,
    ACC_INTERFACE = 0x0200,
    ACC_ABSTRACT = 0x0400,
    ACC_SYNTHETIC = 0x1000,
    ACC_ANNOTATION = 0x2000,
    ACC_ENUM = 0x4000,
    ACC_MODULE = 0x8000
};

template <typename T>
concept PersistentString =
    std::same_as<std::remove_reference_t<T>, const std::string> &&
    std::is_lvalue_reference_v<T&&>;

struct ClassFile {
    Version version;
    uint16_t class_index;
    uint16_t superclass_index;
    constant_pool::ConstantPool constant_pool;
    uint16_t access_flags;
    // TODO(garrett): Interface, field storage
    std::vector<method::Method> methods;
    std::vector<attribute::Attribute> attributes;

    ClassFile() noexcept;

    template <PersistentString S1, PersistentString S2>
    ClassFile(S1&& class_name, S2&& superclass_name)
            : ClassFile() {
        constant_pool.add(
            constant_pool::UTF8Entry{class_name}
        );

        constant_pool.add(constant_pool::ClassEntry{1});
        class_index = 2;

        constant_pool.add(
            constant_pool::UTF8Entry{superclass_name}
        );

        constant_pool.add(constant_pool::ClassEntry{3});
        superclass_index = 4;
    }

    auto name() const -> std::string_view;
    auto superclass() const -> std::string_view;
};

}

#endif // CLASSFILE_H
