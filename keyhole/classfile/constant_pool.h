#ifndef CONSTANT_POOL_H
#define CONSTANT_POOL_H

#include <deque>
#include <format>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace constant_pool {

enum class Tag : uint8_t {
    UTF8 = 1,
    Class = 7,
    MethodReference = 10,
    NameAndType = 12
};

struct ClassEntry {
    uint16_t name_index;
};

struct MethodReferenceEntry {
    uint16_t class_index;
    uint16_t name_and_type_index;
};

struct NameAndTypeEntry {
    uint16_t name_index;
    uint16_t descriptor_index;
};

struct UTF8Entry {
    std::string_view text;
};

using Entry = std::variant<
    ClassEntry,
    MethodReferenceEntry,
    NameAndTypeEntry,
    UTF8Entry
>;

class ConstantPool {
private:
    std::deque<Entry> entries_;
    std::vector<std::optional<size_t>> resolution_table_;
public:
    ConstantPool();
    ConstantPool(std::initializer_list<Entry>);

    auto add(const Entry entry) -> void;
    auto entries() const -> const std::deque<Entry>&;

    template <typename T>
    auto resolve(uint16_t index) const -> const T& {
        if (index >= resolution_table_.size()) {
            throw std::out_of_range(
                std::format("Invalid constant pool access at index {}", index)
            );
        }

        const auto entry_idx = resolution_table_[index];

        if (!entry_idx.has_value()) {
            throw std::runtime_error(
                std::format(
                    "Attempted access to reserved constant pool index {}",
                    index
                )
            );
        }

        const auto& entry = entries_[entry_idx.value()];

        if (!std::holds_alternative<T>(entry)) {
            throw std::runtime_error(
                std::format(
                    "Requested constant pool entry type mismatch at index {}",
                    index
                )
            );
        }

        return std::get<T>(entry);
    };
};

template <typename>
inline constexpr auto always_false_v = false;

constexpr auto tag(const Entry entry) -> Tag {
    return std::visit([](const auto e) constexpr -> Tag {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::same_as<T, ClassEntry>) {
            return Tag::Class;
        } else if constexpr (std::same_as<T, MethodReferenceEntry>) {
            return Tag::MethodReference;
        } else if constexpr (std::same_as<T, NameAndTypeEntry>) {
            return Tag::NameAndType;
        } else if constexpr (std::same_as<T, UTF8Entry>) {
            return Tag::UTF8;
        } else {
            // NOTE(garrett): This will cause a compile failure if we forget an
            // entry value.
            static_assert(
                always_false_v<T>,
                "Unhandled tag generation for CP entry"
            );
        }
    }, entry);
}

constexpr auto name(const Entry entry) -> std::string {
    return std::visit([](const auto e) constexpr -> std::string {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::same_as<T, ClassEntry>) {
            return "Class";
        } else if constexpr (std::same_as<T, MethodReferenceEntry>) {
            return "MethodReference";
        } else if constexpr (std::same_as<T, NameAndTypeEntry>) {
            return "NameAndType";
        } else if constexpr (std::same_as<T, UTF8Entry>) {
            return "UTF-8";
        } else {
            // NOTE(garrett): This will cause a compile failure if we forget an
            // entry value.
            static_assert(
                always_false_v<T>,
                "Unhandled CP entry for entry name"
            );
        }
    }, entry);
}

}

#endif // CONSTANT_POOL_H
