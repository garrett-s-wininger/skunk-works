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

class ClassFile {
private:
    Version version_;
    uint16_t class_index_;
    uint16_t superclass_index_;

    auto set_class_index(uint16_t) -> void;
    auto set_superclass_index(uint16_t) -> void;
    auto set_version(Version) -> void;
public:
    ClassFile() noexcept;
    ClassFile(std::string_view);

    constant_pool::ConstantPool constant_pool;
    uint16_t access_flags;
    std::vector<method::Method> methods;
    std::vector<attribute::Attribute> attributes;

    static auto parse(reader::Reader&) noexcept
        -> std::expected<ClassFile, reader::ParseError>;

    auto dump_contents(sinks::Sink auto& sink) const -> void {
        sink.write(static_cast<uint32_t>(0xCAFEBABE));
        sink.write(static_cast<uint16_t>(version_.minor));
        sink.write(static_cast<uint16_t>(version_.major));
        sink.write(static_cast<uint16_t>(constant_pool.entries().size() + 1));

        constant_pool.dump_contents(sink);

        sink.write(static_cast<uint16_t>(access_flags));
        sink.write(static_cast<uint16_t>(class_index_));
        sink.write(static_cast<uint16_t>(superclass_index_));

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
    auto version() const noexcept -> Version;
};

}

#endif // CLASSFILE_H
