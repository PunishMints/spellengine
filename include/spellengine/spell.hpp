// Spell: runtime object representing an instantiated spell (components + metadata)
#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include "spellengine/spell_component.hpp"
#include "spellengine/spell_context.hpp"

using namespace godot;

class Spell : public RefCounted {
    GDCLASS(Spell, RefCounted)

protected:
    static void _bind_methods();

private:
    TypedArray<Ref<SpellComponent>> components;
    String source_template = "";

public:
    void set_components(const TypedArray<Ref<SpellComponent>> &p_components);
    TypedArray<Ref<SpellComponent>> get_components() const;

    void set_source_template(const String &p);
    String get_source_template() const;

    // Execute the spell using a SpellContext
    void execute(Ref<SpellContext> ctx);
};
