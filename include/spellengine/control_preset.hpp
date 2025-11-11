// ControlPreset resource: reusable control presets for interactive placement
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/classes/packed_scene.hpp>

using namespace godot;

class ControlPreset : public Resource {
    GDCLASS(ControlPreset, Resource)

protected:
    static void _bind_methods();

private:
    String preset_id = "new_preset";
    String type = "vector"; // e.g., vector, position, polygon2d, polygon3d, cylinder, sphere
    Dictionary param_defaults;
    Ref<PackedScene> preview_scene;

public:
    String get_preset_id() const;
    void set_preset_id(const String &p);

    String get_type() const;
    void set_type(const String &t);

    Dictionary get_param_defaults() const;
    void set_param_defaults(const Dictionary &d);

    Ref<PackedScene> get_preview_scene() const;
    void set_preview_scene(const Ref<PackedScene> &s);
};
