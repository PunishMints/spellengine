#include "spellengine/synergy_registry.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

SynergyRegistry *SynergyRegistry::singleton = nullptr;

SynergyRegistry *SynergyRegistry::get_singleton() {
    if (!singleton) singleton = memnew(SynergyRegistry);
    return singleton;
}

void SynergyRegistry::register_synergy(const String &key, const Dictionary &spec) {
    if (key == String()) return;
    synergies[key] = spec;
}

void SynergyRegistry::unregister_synergy(const String &key) {
    if (synergies.has(key)) synergies.erase(key);
}

bool SynergyRegistry::has_synergy(const String &key) const {
    return synergies.has(key);
}

Dictionary SynergyRegistry::get_synergy(const String &key) const {
    if (!has_synergy(key)) return Dictionary();
    return synergies[key];
}

Array SynergyRegistry::get_synergy_keys() const {
    return synergies.keys();
}

void SynergyRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_synergy", "key", "spec"), &SynergyRegistry::register_synergy);
    ClassDB::bind_method(D_METHOD("unregister_synergy", "key"), &SynergyRegistry::unregister_synergy);
    ClassDB::bind_method(D_METHOD("has_synergy", "key"), &SynergyRegistry::has_synergy);
    ClassDB::bind_method(D_METHOD("get_synergy", "key"), &SynergyRegistry::get_synergy);
    ClassDB::bind_method(D_METHOD("get_synergy_keys"), &SynergyRegistry::get_synergy_keys);

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "synergy_keys"), "", "get_synergy_keys");
}
