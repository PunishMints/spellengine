// SynergyRegistry: central registry for designer-defined synergy effects
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

class SynergyRegistry : public Object {
    GDCLASS(SynergyRegistry, Object)

protected:
    static void _bind_methods();

private:
    Dictionary synergies; // key -> Dictionary spec
    static SynergyRegistry *singleton;

public:
    static SynergyRegistry *get_singleton();

    void register_synergy(const String &key, const Dictionary &spec);
    void unregister_synergy(const String &key);
    bool has_synergy(const String &key) const;
    Dictionary get_synergy(const String &key) const;
    Array get_synergy_keys() const;
};
