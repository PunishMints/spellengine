#include "spellengine/editor/aspect_registry_plugin.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_interface.hpp>

using namespace godot;

void AspectRegistryEditorPlugin::_bind_methods() {
    // no methods exposed for now
}

void AspectRegistryEditorPlugin::_enter_tree() {
    // On entering the editor tree, attempt to populate the AspectRegistry from res://aspects
    AspectRegistry *reg = AspectRegistry::get_singleton();
    if (reg) {
        reg->load_all_from_dir("res://aspects", true);
    }
}

void AspectRegistryEditorPlugin::_exit_tree() {
    // no-op for now; we could unregister or clear cached aspects here if desired
}
