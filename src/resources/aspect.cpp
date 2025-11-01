#include "spellengine/aspect.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

TypedArray<Ref<SpellComponent>> Aspect::get_components() const {
    return components;
}

void Aspect::set_components(const TypedArray<Ref<SpellComponent>> &p_components) {
    components = p_components;
}

String Aspect::get_name() const { return name; }
void Aspect::set_name(const String &p_name) { name = p_name; }

String Aspect::get_description() const { return description; }
void Aspect::set_description(const String &p_description) { description = p_description; }

void Aspect::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_components"), &Aspect::get_components);
    ClassDB::bind_method(D_METHOD("set_components", "components"), &Aspect::set_components);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "components"), "set_components", "get_components");

    ClassDB::bind_method(D_METHOD("get_name"), &Aspect::get_name);
    ClassDB::bind_method(D_METHOD("set_name", "name"), &Aspect::set_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");

    ClassDB::bind_method(D_METHOD("get_description"), &Aspect::get_description);
    ClassDB::bind_method(D_METHOD("set_description", "description"), &Aspect::set_description);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "description"), "set_description", "get_description");
}
