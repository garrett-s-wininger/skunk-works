#include "classfile.h"

classfile::ClassFile::ClassFile() noexcept
    : version_{classfile::Version{55u, 0u}}
    , class_index_{0u}
    , superclass_index_{0u}
    // NOTE(garrett): Public members after here
    , constant_pool(constant_pool::ConstantPool{})
    , access_flags(0x0021)
    , methods(std::vector<method::Method>{})
    , attributes(std::vector<attribute::Attribute>{}) {}

classfile::ClassFile::ClassFile(std::string_view class_name)
        : classfile::ClassFile::ClassFile() {
    constant_pool.add(
        constant_pool::UTF8Entry{std::string{class_name}}
    );

    constant_pool.add(constant_pool::ClassEntry{1});
    class_index_ = 2;

    constant_pool.add(
        constant_pool::UTF8Entry{std::string{"java/lang/Object"}}
    );

    constant_pool.add(constant_pool::ClassEntry{3});
    superclass_index_ = 4;
}

auto classfile::ClassFile::name() const -> std::string_view {
    const auto& class_entry = constant_pool.resolve<constant_pool::ClassEntry>(
        class_index_
    );

    return constant_pool.resolve<constant_pool::UTF8Entry>(
        class_entry.name_index
    ).text;
}

auto classfile::ClassFile::parse(reader::Reader& reader) noexcept
        -> std::expected<ClassFile, reader::ParseError> {
    const auto header = reader.read_bytes(sizeof(uint64_t) + sizeof(uint16_t));

    if (!header) {
        return std::unexpected(header.error());
    }

    auto header_reader = reader::Reader{header.value()};

    if (header_reader.read_unchecked<uint32_t>() != 0xCAFEBABE) {
        return std::unexpected(reader::ParseError::InvalidMagic);
    }

    const auto minor = header_reader.read_unchecked<uint16_t>();
    const auto major = header_reader.read_unchecked<uint16_t>();

    auto result = classfile::ClassFile{};
    result.set_version(classfile::Version{major, minor});

    const auto pool = constant_pool::ConstantPool::parse(
        reader,
        // NOTE(garrett): Classfile contains the actual count, plus one
        header_reader.read_unchecked<uint16_t>() - 1
    );

    if (!pool) {
        return std::unexpected(pool.error());
    }

    result.constant_pool = pool.value();

    const auto access_and_class_metadata = reader.read_bytes(sizeof(uint64_t));

    if (!access_and_class_metadata) {
        return std::unexpected(access_and_class_metadata.error());
    }

    auto metadata_reader = reader::Reader{access_and_class_metadata.value()};
    result.access_flags = metadata_reader.read_unchecked<uint16_t>();
    result.set_class_index(metadata_reader.read_unchecked<uint16_t>());
    result.set_superclass_index(metadata_reader.read_unchecked<uint16_t>());

    const auto interface_count = metadata_reader.read_unchecked<uint16_t>();

    // TODO(garrett): Interface parsing
    if (interface_count > 0) {
        return std::unexpected(reader::ParseError::UnsupportedEntity);
    }

    const auto fields_count = reader.read<uint16_t>();

    // TODO(garrett): Field parsing
    if (!fields_count) {
        return std::unexpected(fields_count.error());
    } else if (fields_count.value() > 0) {
        return std::unexpected(reader::ParseError::UnsupportedEntity);
    }

    const auto methods_count = reader.read<uint16_t>();

    if (!methods_count) {
        return std::unexpected(methods_count.error());
    }

    result.methods = std::vector<method::Method>{};

    for (auto i = 0u; i < methods_count.value(); ++i) {
        const auto method = method::Method::parse(reader);

        if (!method) {
            return std::unexpected(method.error());
        }

        result.methods.push_back(method.value());
    }

    const auto attributes_count = reader.read<uint16_t>();

    if (!attributes_count) {
        return std::unexpected(attributes_count.error());
    }

    result.attributes = std::vector<attribute::Attribute>{};

    for (auto i = 0uz; i < attributes_count.value(); ++i) {
        const auto attribute = attribute::Attribute::parse(reader);

        if (!attribute) {
            return std::unexpected(attribute.error());
        }

        result.attributes.push_back(attribute.value());
    }

    return result;
}

auto classfile::ClassFile::set_class_index(uint16_t index) -> void {
    class_index_ = index;
}

auto classfile::ClassFile::set_superclass_index(uint16_t index) -> void {
    superclass_index_ = index;
}

auto classfile::ClassFile::set_version(classfile::Version version) -> void {
    version_ = version;
}

auto classfile::ClassFile::superclass() const -> std::string_view {
    const auto& class_entry = constant_pool.resolve<constant_pool::ClassEntry>(
        superclass_index_
    );

    return constant_pool.resolve<constant_pool::UTF8Entry>(
        class_entry.name_index
    ).text;
}

auto classfile::ClassFile::version() const noexcept -> classfile::Version {
    return version_;
}
