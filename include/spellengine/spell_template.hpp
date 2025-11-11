// Public header for SpellTemplate resource
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include "spellengine/spell_component.hpp"
#include "spellengine/control_descriptor.hpp"

using namespace godot;

class SpellTemplate : public Resource {
    GDCLASS(SpellTemplate, Resource)

protected:
    static void _bind_methods();

private:
    String name = "New Spell";
    String description = "";
    TypedArray<Ref<SpellComponent>> components;
    TypedArray<Ref<ControlDescriptor>> controls;

public:
    String get_name() const;
    void set_name(const String &p_name);

    String get_description() const;
    void set_description(const String &p_description);

    TypedArray<Ref<SpellComponent>> get_components() const;
    void set_components(const TypedArray<Ref<SpellComponent>> &p_components);

    TypedArray<Ref<ControlDescriptor>> get_controls() const;
    void set_controls(const TypedArray<Ref<ControlDescriptor>> &p_controls);
};
