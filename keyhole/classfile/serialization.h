#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "attribute.h"
#include "method.h"
#include "sinks.h"

namespace serialization {

auto serialize(sinks::Sink auto& sink, attribute::Attribute attribute) -> void {
    sink.write(attribute.name_index);
    sink.write(static_cast<uint32_t>(attribute.data.size()));

    for (const auto byte : attribute.data) {
        sink.write(static_cast<uint8_t>(byte));
    }
}

auto serialize(sinks::Sink auto& sink, method::Method method) -> void {
    sink.write(method.access_flags);
    sink.write(method.name_index);
    sink.write(method.descriptor_index);

    for (const auto& attribute : method.attributes) {
        serialize(sink, attribute);
    }
}

}

#endif // SERIALIZATION_H
