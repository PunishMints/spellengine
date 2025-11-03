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
    // Helpers to register/load synergies from files.
    // Register spec under key by loading a JSON file or resource file that exposes a `spec` Dictionary property.
    bool register_from_path(const String &key, const String &resource_path);
    int load_all_from_dir(const String &dir_path, bool recursive = true);
};
