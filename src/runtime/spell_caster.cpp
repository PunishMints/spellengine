#include "spellengine/spell_caster.hpp"

#include <godot_cpp/core/class_db.hpp>
// Note: aspect defaults are applied during spell composition in SpellEngine, not here.

using namespace godot;

double SpellCaster::get_mana(const String &aspect) const {
    if (aspect_mana.has(aspect)) return (double)aspect_mana[aspect];
    return 0.0;
}

void SpellCaster::set_mana(const String &aspect, double amount) {
    aspect_mana[aspect] = amount;
}

void SpellCaster::add_mana(const String &aspect, double amount) {
    double cur = get_mana(aspect);
    aspect_mana[aspect] = cur + amount;
}

bool SpellCaster::can_deduct(const String &aspect, double amount) const {
    double cur = get_mana(aspect);
    return cur >= amount - 1e-9;
}

bool SpellCaster::deduct_mana(const String &aspect, double amount) {
    if (!can_deduct(aspect, amount)) return false;
    double cur = get_mana(aspect);
    aspect_mana[aspect] = cur - amount;
    return true;
}

Array SpellCaster::get_assigned_aspects() const {
    return assigned_aspects;
}

void SpellCaster::set_assigned_aspects(const Array &a) {
    assigned_aspects = a;
}

int SpellCaster::get_scaler_merge_mode() const {
    return scaler_merge_mode;
}

void SpellCaster::set_scaler_merge_mode(int mode) {
    scaler_merge_mode = mode;
}

Dictionary SpellCaster::get_aspect_scalers() const {
    return aspect_scalers;
}

void SpellCaster::set_aspect_scalers(const Dictionary &s) {
    aspect_scalers = s;
}

double SpellCaster::get_scaler(const String &aspect, const String &key) const {
    if (!aspect_scalers.has(aspect)) return 1.0;
    Variant v = aspect_scalers[aspect];
    if (v.get_type() != Variant::DICTIONARY) return 1.0;
    Dictionary dict = v;
    if (!dict.has(key)) return 1.0;
    Variant sv = dict[key];
    if (sv.get_type() == Variant::INT || sv.get_type() == Variant::FLOAT) return (double)sv;
    return 1.0;
}

void SpellCaster::set_scaler(const String &aspect, const String &key, double value) {
    Dictionary dict;
    if (aspect_scalers.has(aspect)) {
        Variant v = aspect_scalers[aspect];
        if (v.get_type() == Variant::DICTIONARY) dict = v;
    }
    dict[key] = value;
    aspect_scalers[aspect] = dict;
}

void SpellCaster::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_mana", "aspect"), &SpellCaster::get_mana);
    ClassDB::bind_method(D_METHOD("set_mana", "aspect", "amount"), &SpellCaster::set_mana);
    ClassDB::bind_method(D_METHOD("add_mana", "aspect", "amount"), &SpellCaster::add_mana);
    ClassDB::bind_method(D_METHOD("can_deduct", "aspect", "amount"), &SpellCaster::can_deduct);
    ClassDB::bind_method(D_METHOD("deduct_mana", "aspect", "amount"), &SpellCaster::deduct_mana);

    ClassDB::bind_method(D_METHOD("get_assigned_aspects"), &SpellCaster::get_assigned_aspects);
    ClassDB::bind_method(D_METHOD("set_assigned_aspects", "aspects"), &SpellCaster::set_assigned_aspects);

    ClassDB::bind_method(D_METHOD("get_aspect_scalers"), &SpellCaster::get_aspect_scalers);
    ClassDB::bind_method(D_METHOD("set_aspect_scalers", "scalers"), &SpellCaster::set_aspect_scalers);
    ClassDB::bind_method(D_METHOD("get_scaler", "aspect", "key"), &SpellCaster::get_scaler);
    ClassDB::bind_method(D_METHOD("set_scaler", "aspect", "key", "value"), &SpellCaster::set_scaler);

    ClassDB::bind_method(D_METHOD("get_scaler_merge_mode"), &SpellCaster::get_scaler_merge_mode);
    ClassDB::bind_method(D_METHOD("set_scaler_merge_mode", "mode"), &SpellCaster::set_scaler_merge_mode);

    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "aspect_mana"), "", "get_assigned_aspects");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "assigned_aspects"), "set_assigned_aspects", "get_assigned_aspects");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "aspect_scalers"), "set_aspect_scalers", "get_aspect_scalers");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "scaler_merge_mode"), "set_scaler_merge_mode", "get_scaler_merge_mode");
}
