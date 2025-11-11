#include "spellengine/control_preset.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

String ControlPreset::get_preset_id() const { return preset_id; }
void ControlPreset::set_preset_id(const String &p) { preset_id = p; }

String ControlPreset::get_type() const { return type; }
void ControlPreset::set_type(const String &t) { type = t; }

Dictionary ControlPreset::get_param_defaults() const { return param_defaults; }
void ControlPreset::set_param_defaults(const Dictionary &d) { param_defaults = d; }

Ref<PackedScene> ControlPreset::get_preview_scene() const { return preview_scene; }
void ControlPreset::set_preview_scene(const Ref<PackedScene> &s) { preview_scene = s; }

void ControlPreset::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_preset_id"), &ControlPreset::get_preset_id);
    ClassDB::bind_method(D_METHOD("set_preset_id", "id"), &ControlPreset::set_preset_id);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "preset_id"), "set_preset_id", "get_preset_id");

    ClassDB::bind_method(D_METHOD("get_type"), &ControlPreset::get_type);
    ClassDB::bind_method(D_METHOD("set_type", "type"), &ControlPreset::set_type);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");

    ClassDB::bind_method(D_METHOD("get_param_defaults"), &ControlPreset::get_param_defaults);
    ClassDB::bind_method(D_METHOD("set_param_defaults", "defaults"), &ControlPreset::set_param_defaults);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "param_defaults"), "set_param_defaults", "get_param_defaults");

    ClassDB::bind_method(D_METHOD("get_preview_scene"), &ControlPreset::get_preview_scene);
    ClassDB::bind_method(D_METHOD("set_preview_scene", "scene"), &ControlPreset::set_preview_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "preview_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_preview_scene", "get_preview_scene");
}
