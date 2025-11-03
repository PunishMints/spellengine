// Registry for SpellComponent resources
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include "spellengine/spell_component.hpp"

using namespace godot;

class SpellComponentRegistry : public Object {
    GDCLASS(SpellComponentRegistry, Object)

protected:
    static void _bind_methods();

private:
    Dictionary components; // id -> Ref<SpellComponent>
    static SpellComponentRegistry *singleton;

public:
    static SpellComponentRegistry *get_singleton();

    void register_component(const String &id, Ref<SpellComponent> comp);
    void unregister_component(const String &id);
    Array get_component_ids() const;
    bool has_component(const String &id) const;
    Ref<SpellComponent> get_component(const String &id) const;

    // Resource helpers
    bool register_from_path(const String &id, const String &resource_path);
    int load_all_from_dir(const String &dir_path, bool recursive = true);
};
