// IExecutor base class
#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include "spellengine/spell_context.hpp"
#include "spellengine/spell_component.hpp"

using namespace godot;

class IExecutor : public RefCounted {
    GDCLASS(IExecutor, RefCounted)

protected:
    static void _bind_methods();

public:
    virtual void execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) = 0;

    // Each executor must provide its own id. This allows executors to self-identify
    // and be auto-registered by the module initialization code.
    virtual String get_executor_id() const = 0;
};
