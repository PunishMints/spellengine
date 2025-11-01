// Minimal executor registry: stores known executor ids for component execution
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include "spellengine/executor_base.hpp"

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
};
