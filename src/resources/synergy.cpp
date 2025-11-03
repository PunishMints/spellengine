#include "spellengine/synergy.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

Dictionary Synergy::get_spec() const { return spec; }
void Synergy::set_spec(const Dictionary &p) { spec = p; }

void Synergy::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_spec"), &Synergy::get_spec);
    ClassDB::bind_method(D_METHOD("set_spec", "spec"), &Synergy::set_spec);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "spec"), "set_spec", "get_spec");
}
