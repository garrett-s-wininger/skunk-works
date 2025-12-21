#ifndef CLASSFILE_H
#define CLASSFILE_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include "attribute.h"
#include "constant_pool.h"
#include "endian.h"
#include "method.h"
#include "parsing.h"
#include "reader.h"
#include "sinks.h"

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

    static auto parse(reader::Reader&) noexcept
        -> std::expected<ClassFile, parsing::Error>;

    auto dump_contents(sinks::Sink auto& sink) const -> void {
        sink.write(static_cast<uint32_t>(0xCAFEBABE));
        sink.write(static_cast<uint16_t>(version.minor));
        sink.write(static_cast<uint16_t>(version.major));
        sink.write(static_cast<uint16_t>(constant_pool.entries().size() + 1));

        constant_pool.dump_contents(sink);

        sink.write(static_cast<uint16_t>(access_flags));
        sink.write(static_cast<uint16_t>(class_index));
        sink.write(static_cast<uint16_t>(superclass_index));

        // TODO(garrett): Write interface entries
        sink.write(static_cast<uint16_t>(0x0000));

        // TODO(garrett): Write field entries
        sink.write(static_cast<uint16_t>(0x0000));

        sink.write(static_cast<uint16_t>(methods.size()));
        // TODO(garrett): Write method entries

        sink.write(static_cast<uint16_t>(attributes.size()));
        // TODO(garrett): Write attribute entries
    }

    auto name() const -> std::string_view;
    auto superclass() const -> std::string_view;
};

}

#endif // CLASSFILE_H
