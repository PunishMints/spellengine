// SpellEngine: core factory / composition / execution orchestration
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include "spellengine/aspect.hpp"
#include "spellengine/spell.hpp"

class SpellCaster;

using namespace godot;

class SpellEngine : public Object {
    GDCLASS(SpellEngine, Object)

protected:
    static void _bind_methods();

public:
    // Build a Spell from a list of aspects. Currently this simply aggregates components.
    Ref<Spell> build_spell_from_aspects(const TypedArray<Ref<Aspect>> &aspects);

    // Execute a spell with a given context
    void execute_spell(Ref<Spell> spell, Ref<SpellContext> ctx);

    // Resolve a single component's parameters given the casting aspects (simple weighted rules).
    // Optionally provide the SpellCaster to apply per-aspect caster scalers when computing final numeric values.
    Dictionary resolve_component_params(Ref<SpellComponent> component, const Array &casting_aspects, SpellCaster *caster = nullptr);
};
