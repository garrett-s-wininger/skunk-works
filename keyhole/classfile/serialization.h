#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "attribute.h"
#include "classfile.h"
#include "constant_pool.h"
#include "method.h"
#include "sinks.h"

namespace serialization {

auto serialize(sinks::Sink auto& sink, const attribute::Attribute& attribute) -> void {
    sink.write(attribute.name_index);
    sink.write(static_cast<uint32_t>(attribute.data.size()));

    for (const auto byte : attribute.data) {
        sink.write(static_cast<uint8_t>(byte));
    }
}

auto serialize(sinks::Sink auto& sink, const constant_pool::ClassEntry entry) -> void {
    sink.write(static_cast<uint8_t>(constant_pool::tag(entry)));
    sink.write(entry.name_index);
}

auto serialize(sinks::Sink auto& sink, const method::Method& method) -> void {
    sink.write(method.access_flags);
    sink.write(method.name_index);
    sink.write(method.descriptor_index);

    for (const auto& attribute : method.attributes) {
        serialize(sink, attribute);
    }
}

auto serialize(sinks::Sink auto& sink, const constant_pool::MethodReferenceEntry entry)
        -> void {
    sink.write(static_cast<uint8_t>(constant_pool::tag(entry)));
    sink.write(entry.class_index);
    sink.write(entry.name_and_type_index);
}

auto serialize(sinks::Sink auto& sink, constant_pool::NameAndTypeEntry entry) -> void {
    sink.write(static_cast<uint8_t>(constant_pool::tag(entry)));
    sink.write(entry.name_index);
    sink.write(entry.descriptor_index);
}

auto serialize(sinks::Sink auto& sink, constant_pool::UTF8Entry entry) -> void {
    sink.write(static_cast<uint8_t>(constant_pool::tag(entry)));
    sink.write(static_cast<uint16_t>(entry.text.size()));

    for (const auto byte : entry.text) {
        sink.write(static_cast<uint8_t>(byte));
    }
}

auto serialize(sinks::Sink auto& sink, const constant_pool::ConstantPool& pool)
        -> void {
    for (const auto& entry : pool.entries()) {
        std::visit([&sink](const auto& e){
            serialize(sink, e);
        }, entry);
    }
}

auto serialize(sinks::Sink auto& sink, const classfile::ClassFile& klass) -> void {
    sink.write(static_cast<uint32_t>(0xCAFEBABE));
    sink.write(static_cast<uint16_t>(klass.version.minor));
    sink.write(static_cast<uint16_t>(klass.version.major));
    sink.write(static_cast<uint16_t>(klass.constant_pool.entries().size() + 1));

    serialization::serialize(sink, klass.constant_pool);

    sink.write(static_cast<uint16_t>(klass.access_flags));
    sink.write(static_cast<uint16_t>(klass.class_index));
    sink.write(static_cast<uint16_t>(klass.superclass_index));

    // TODO(garrett): Write interface entries
    sink.write(static_cast<uint16_t>(0x0000));

    // TODO(garrett): Write field entries
    sink.write(static_cast<uint16_t>(0x0000));

    sink.write(static_cast<uint16_t>(klass.methods.size()));
    // TODO(garrett): Write method entries

    sink.write(static_cast<uint16_t>(klass.attributes.size()));
    // TODO(garrett): Write attribute entries
}

}

#endif // SERIALIZATION_H
