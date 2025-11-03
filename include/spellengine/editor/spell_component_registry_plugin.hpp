// Editor plugin that scans 'res://spell_components' and registers found SpellComponent resources
#pragma once

#include <godot_cpp/classes/editor_plugin.hpp>
#include "spellengine/spell_component_registry.hpp"

using namespace godot;

class SpellComponentRegistryEditorPlugin : public EditorPlugin {
    GDCLASS(SpellComponentRegistryEditorPlugin, EditorPlugin)

protected:
    static void _bind_methods();

public:
    void _enter_tree() override;
    void _exit_tree() override;
};
