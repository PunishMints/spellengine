// Editor plugin that scans 'res://aspects' and registers found Aspect resources
#pragma once

#include <godot_cpp/classes/editor_plugin.hpp>
#include "spellengine/aspect_registry.hpp"

using namespace godot;

class AspectRegistryEditorPlugin : public EditorPlugin {
    GDCLASS(AspectRegistryEditorPlugin, EditorPlugin)

protected:
    static void _bind_methods();

public:
    void _enter_tree() override;
    void _exit_tree() override;
};
