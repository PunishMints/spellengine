#include "spellengine/executor_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

ExecutorRegistry *ExecutorRegistry::singleton = nullptr;

ExecutorRegistry *ExecutorRegistry::get_singleton() {
    if (!singleton) {
        singleton = memnew(ExecutorRegistry);
    }
    return singleton;
}

void ExecutorRegistry::register_executor(const String &id, Ref<IExecutor> executor) {
    if (!has_executor(id) && executor.is_valid()) {
        executors[id] = executor;
    }
}

void ExecutorRegistry::unregister_executor(const String &id) {
    if (has_executor(id)) {
        executors.erase(id);
    }
}

Array ExecutorRegistry::get_executor_ids() const {
    Array keys = executors.keys();
    return keys;
}

bool ExecutorRegistry::has_executor(const String &id) const {
    return executors.has(id);
}

Ref<IExecutor> ExecutorRegistry::get_executor(const String &id) const {
    if (!has_executor(id)) return Ref<IExecutor>();
    return executors[id];
}

void ExecutorRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_executor", "id", "executor"), &ExecutorRegistry::register_executor);
    ClassDB::bind_method(D_METHOD("unregister_executor", "id"), &ExecutorRegistry::unregister_executor);
    ClassDB::bind_method(D_METHOD("get_executor_ids"), &ExecutorRegistry::get_executor_ids);
    ClassDB::bind_method(D_METHOD("has_executor", "id"), &ExecutorRegistry::has_executor);
    ClassDB::bind_method(D_METHOD("get_executor", "id"), &ExecutorRegistry::get_executor);

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "executor_ids"), "", "get_executor_ids");
}
