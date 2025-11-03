// Minimal executor registry: stores known executor ids for component execution
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include "spellengine/executor_base.hpp"
#include <vector>
#include <functional>

using namespace godot;

class ExecutorRegistry : public Object {
    GDCLASS(ExecutorRegistry, Object)

protected:
    static void _bind_methods();

private:
    // map executor id -> executor instance
    Dictionary executors;
    static ExecutorRegistry *singleton;

public:
    // Singleton accessor
    static ExecutorRegistry *get_singleton();

    void register_executor(const String &id, Ref<IExecutor> executor);
    void unregister_executor(const String &id);
    Array get_executor_ids() const;
    bool has_executor(const String &id) const;
    Ref<IExecutor> get_executor(const String &id) const;

    // Register a factory for an executor. Factories are stored and later
    // instantiated during module init via register_all_factories(). This allows
    // executor cpp files to register themselves without editing register_types.cpp.
    static bool register_factory_static(const std::function<Ref<IExecutor>()> &factory);

    // Instantiate and register all known executor factories. Called from module init.
    static void register_all_factories();
};

// Macro to place in executor cpp files to register a factory for that executor
#define REGISTER_EXECUTOR_FACTORY(EXEC_CLASS) \
    static bool _executor_factory_reg_##EXEC_CLASS = ExecutorRegistry::register_factory_static([]() -> Ref<IExecutor> { return Ref<IExecutor>(memnew(EXEC_CLASS)); });
