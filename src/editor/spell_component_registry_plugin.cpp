#include "spellengine/editor/spell_component_registry_plugin.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SpellComponentRegistryEditorPlugin::_bind_methods() {
    // no methods for now
}

void SpellComponentRegistryEditorPlugin::_enter_tree() {
    SpellComponentRegistry *reg = SpellComponentRegistry::get_singleton();
    if (reg) {
        reg->load_all_from_dir("res://spell_components", true);
    }
}

void SpellComponentRegistryEditorPlugin::_exit_tree() {
}
