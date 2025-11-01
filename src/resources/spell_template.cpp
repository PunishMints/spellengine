#include "spellengine/spell_template.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

String SpellTemplate::get_name() const {
    return name;
}

void SpellTemplate::set_name(const String &p_name) {
    name = p_name;
}

String SpellTemplate::get_description() const {
    return description;
}

void SpellTemplate::set_description(const String &p_description) {
    description = p_description;
}

TypedArray<Ref<SpellComponent>> SpellTemplate::get_components() const {
    return components;
}

void SpellTemplate::set_components(const TypedArray<Ref<SpellComponent>> &p_components) {
    components = p_components;
}

void SpellTemplate::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_name"), &SpellTemplate::get_name);
    ClassDB::bind_method(D_METHOD("set_name", "name"), &SpellTemplate::set_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");

    ClassDB::bind_method(D_METHOD("get_description"), &SpellTemplate::get_description);
    ClassDB::bind_method(D_METHOD("set_description", "description"), &SpellTemplate::set_description);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "description"), "set_description", "get_description");

    ClassDB::bind_method(D_METHOD("get_components"), &SpellTemplate::get_components);
    ClassDB::bind_method(D_METHOD("set_components", "components"), &SpellTemplate::set_components);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "components"), "set_components", "get_components");
}
