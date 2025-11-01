#include "spellengine/spell_component.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

String SpellComponent::get_executor_id() const {
    return executor_id;
}

void SpellComponent::set_executor_id(const String &p_id) {
    executor_id = p_id;
}

int SpellComponent::get_priority() const {
    return priority;
}

void SpellComponent::set_priority(int p) {
    priority = p;
}

double SpellComponent::get_cost() const {
    return cost;
}

void SpellComponent::set_cost(double c) {
    cost = c;
}

Dictionary SpellComponent::get_params() const {
    // Backwards-compatible accessor for base_params
    return base_params;
}

void SpellComponent::set_params(const Dictionary &p_params) {
    base_params = p_params;
}

Dictionary SpellComponent::get_base_params() const {
    return base_params;
}

void SpellComponent::set_base_params(const Dictionary &p) {
    base_params = p;
}

Dictionary SpellComponent::get_aspects_contributions() const {
    return aspects_contributions;
}

void SpellComponent::set_aspects_contributions(const Dictionary &d) {
    aspects_contributions = d;
}

Dictionary SpellComponent::get_aspect_modifiers() const {
    return aspect_modifiers;
}

void SpellComponent::set_aspect_modifiers(const Dictionary &d) {
    aspect_modifiers = d;
}

Dictionary SpellComponent::get_synergy_modifiers() const {
    return synergy_modifiers;
}

void SpellComponent::set_synergy_modifiers(const Dictionary &d) {
    synergy_modifiers = d;
}

void SpellComponent::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_executor_id"), &SpellComponent::get_executor_id);
    ClassDB::bind_method(D_METHOD("set_executor_id", "id"), &SpellComponent::set_executor_id);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "executor_id"), "set_executor_id", "get_executor_id");

    ClassDB::bind_method(D_METHOD("get_priority"), &SpellComponent::get_priority);
    ClassDB::bind_method(D_METHOD("set_priority", "priority"), &SpellComponent::set_priority);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "priority"), "set_priority", "get_priority");

    ClassDB::bind_method(D_METHOD("get_cost"), &SpellComponent::get_cost);
    ClassDB::bind_method(D_METHOD("set_cost", "cost"), &SpellComponent::set_cost);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cost"), "set_cost", "get_cost");

    ClassDB::bind_method(D_METHOD("get_params"), &SpellComponent::get_params);
    ClassDB::bind_method(D_METHOD("set_params", "params"), &SpellComponent::set_params);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "params"), "set_params", "get_params");

    ClassDB::bind_method(D_METHOD("get_base_params"), &SpellComponent::get_base_params);
    ClassDB::bind_method(D_METHOD("set_base_params", "base_params"), &SpellComponent::set_base_params);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "base_params"), "set_base_params", "get_base_params");

    ClassDB::bind_method(D_METHOD("get_aspects_contributions"), &SpellComponent::get_aspects_contributions);
    ClassDB::bind_method(D_METHOD("set_aspects_contributions", "contribs"), &SpellComponent::set_aspects_contributions);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "aspects_contributions"), "set_aspects_contributions", "get_aspects_contributions");

    ClassDB::bind_method(D_METHOD("get_aspect_modifiers"), &SpellComponent::get_aspect_modifiers);
    ClassDB::bind_method(D_METHOD("set_aspect_modifiers", "mods"), &SpellComponent::set_aspect_modifiers);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "aspect_modifiers"), "set_aspect_modifiers", "get_aspect_modifiers");

    ClassDB::bind_method(D_METHOD("get_synergy_modifiers"), &SpellComponent::get_synergy_modifiers);
    ClassDB::bind_method(D_METHOD("set_synergy_modifiers", "mods"), &SpellComponent::set_synergy_modifiers);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "synergy_modifiers"), "set_synergy_modifiers", "get_synergy_modifiers");
}
