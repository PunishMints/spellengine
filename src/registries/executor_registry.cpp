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

// Implementation of factory registration. We keep factories in a function-static
// vector to avoid static-initialization-order issues.
static std::vector<std::function<Ref<IExecutor>()>> &get_executor_factories() {
    static std::vector<std::function<Ref<IExecutor>()>> factories;
    return factories;
}

bool ExecutorRegistry::register_factory_static(const std::function<Ref<IExecutor>()> &factory) {
    auto &f = get_executor_factories();
    f.push_back(factory);
    return true;
}

void ExecutorRegistry::register_all_factories() {
    auto &f = get_executor_factories();
    ExecutorRegistry *reg = ExecutorRegistry::get_singleton();
    for (size_t i = 0; i < f.size(); ++i) {
        Ref<IExecutor> inst = f[i]();
        if (!inst.is_valid()) continue;
        String id = inst->get_executor_id();
        if (id.is_empty()) continue;
        reg->register_executor(id, inst);
        // Diagnostic trace for factory registration
        UtilityFunctions::print(String("[spellengine] ExecutorRegistry registered executor_factory -> ") + id);
    }
}

void ExecutorRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_executor", "id", "executor"), &ExecutorRegistry::register_executor);
    ClassDB::bind_method(D_METHOD("unregister_executor", "id"), &ExecutorRegistry::unregister_executor);
    ClassDB::bind_method(D_METHOD("get_executor_ids"), &ExecutorRegistry::get_executor_ids);
    ClassDB::bind_method(D_METHOD("has_executor", "id"), &ExecutorRegistry::has_executor);
    ClassDB::bind_method(D_METHOD("get_executor", "id"), &ExecutorRegistry::get_executor);

    // Note: singleton accessor is not bound here. Editor tooling can read
    // executor ids/schemas from ProjectSettings (written by module init).

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "executor_ids"), "", "get_executor_ids");
}
