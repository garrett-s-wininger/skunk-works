#include "constant_pool.h"

constant_pool::ConstantPool::ConstantPool()
        : entries_(std::deque<constant_pool::Entry>{})
        , resolution_table_(std::vector<std::optional<size_t>>{}) {
    // NOTE(garrett): Index zero is reserved, access should be 1-indexed so we
    // can grab data directly from other classfile references
    resolution_table_.push_back(std::nullopt);
}

constant_pool::ConstantPool::ConstantPool(std::initializer_list<constant_pool::Entry> entries)
        : constant_pool::ConstantPool::ConstantPool() {
    for (const auto& entry : entries) {
        add(entry);
    }
}

auto constant_pool::ConstantPool::add(const Entry entry) -> void {
    resolution_table_.push_back(entries_.size());
    entries_.push_back(entry);
}

auto constant_pool::ConstantPool::entries() const -> const std::deque<Entry>& {
    return entries_;
}
