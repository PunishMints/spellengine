#include "spellengine/spell.hpp"


#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
using namespace godot;

void Spell::set_components(const TypedArray<Ref<SpellComponent>> &p_components) {
    components = p_components;
}

TypedArray<Ref<SpellComponent>> Spell::get_components() const {
    return components;
}

void Spell::set_source_template(const String &p) {
    source_template = p;
}

String Spell::get_source_template() const {
    return source_template;
}

void Spell::execute(Ref<SpellContext> ctx) {
    if (!ctx.is_valid()) {
        UtilityFunctions::print("Spell::execute called with invalid context");
        return;
    }

    for (int i = 0; i < components.size(); ++i) {
        Ref<SpellComponent> comp = components[i];
        if (comp.is_valid()) {
            UtilityFunctions::print(String("Spell::execute (stub) component: ") + comp->get_executor_id());
        }
    }
}

void Spell::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_components", "components"), &Spell::set_components);
    ClassDB::bind_method(D_METHOD("get_components"), &Spell::get_components);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "components"), "set_components", "get_components");

    ClassDB::bind_method(D_METHOD("set_source_template", "source"), &Spell::set_source_template);
    ClassDB::bind_method(D_METHOD("get_source_template"), &Spell::get_source_template);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_template"), "set_source_template", "get_source_template");

    ClassDB::bind_method(D_METHOD("execute", "context"), &Spell::execute);
}
