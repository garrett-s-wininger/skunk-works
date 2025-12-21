#include "classfile.h"

classfile::ClassFile::ClassFile() noexcept
    : version{classfile::Version{55u, 0u}}
    , class_index{0u}
    , superclass_index{0u}
    // NOTE(garrett): Public members after here
    , constant_pool(constant_pool::ConstantPool{})
    , access_flags(0x0021)
    , methods(std::vector<method::Method>{})
    , attributes(std::vector<attribute::Attribute>{}) {}

auto classfile::ClassFile::name() const -> std::string_view {
    const auto& class_entry = constant_pool.resolve<constant_pool::ClassEntry>(
        class_index
    );

    return constant_pool.resolve<constant_pool::UTF8Entry>(
        class_entry.name_index
    ).text;
}

auto classfile::ClassFile::superclass() const -> std::string_view {
    const auto& class_entry = constant_pool.resolve<constant_pool::ClassEntry>(
        superclass_index
    );

    return constant_pool.resolve<constant_pool::UTF8Entry>(
        class_entry.name_index
    ).text;
}
