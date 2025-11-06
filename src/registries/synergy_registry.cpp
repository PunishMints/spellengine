#include "spellengine/synergy_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "spellengine/synergy.hpp"

using namespace godot;

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
    ClassDB::bind_method(D_METHOD("register_from_path", "key", "resource_path"), &SynergyRegistry::register_from_path);
    ClassDB::bind_method(D_METHOD("load_all_from_dir", "dir_path", "recursive"), &SynergyRegistry::load_all_from_dir);

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "synergy_keys"), "", "get_synergy_keys");
}

bool SynergyRegistry::register_from_path(const String &key, const String &resource_path) {
    // Support JSON files or Resource files with a `spec` Dictionary property
    String lower = resource_path.to_lower();
    // Only support Resource-based specs (.tres, .res) for now. Resource should expose a `spec` Dictionary property.
    Ref<Resource> r = ResourceLoader::get_singleton()->load(resource_path);
    using namespace godot;
    if (!r.is_valid()) {
        UtilityFunctions::print(String("SynergyRegistry: failed to load resource: ") + resource_path);
        return false;
    }
    // Prefer strongly-typed Synergy resource
    Synergy *sres = Object::cast_to<Synergy>(r.ptr());
    if (sres) {
        Dictionary spec = sres->get_spec();
        UtilityFunctions::print(String("SynergyRegistry: loaded Synergy resource spec from: ") + resource_path);
        UtilityFunctions::print(spec);
        register_synergy(key.to_lower(), spec);
        // If the Synergy resource declares component_aspects, also register an
        // alias key formed by sorting and lowercasing the aspect names joined by '+'.
        if (spec.has("component_aspects")) {
            Variant cav = spec["component_aspects"];
            if (cav.get_type() == Variant::ARRAY) {
                Array carr = cav;
                Array lower_aspects;
                for (int i = 0; i < carr.size(); ++i) {
                    Variant av = carr[i];
                    if (av.get_type() != Variant::STRING) continue;
                    String as = ((String)av).to_lower();
                    lower_aspects.push_back(as);
                }
                lower_aspects.sort();
                String alias = "";
                for (int i = 0; i < lower_aspects.size(); ++i) {
                    if (i) alias += "+";
                    alias += (String)lower_aspects[i];
                }
                if (alias != String() && !has_synergy(alias)) {
                    register_synergy(alias, spec);
                    UtilityFunctions::print(String("SynergyRegistry: registered alias key for resource '") + key + "' -> '" + alias + "'");
                }
            }
        }
        return true;
    }

    // Fallback: try to read a `spec` property on generic Resource
    Variant spec = Object::cast_to<Object>(r.ptr())->get("spec");
    int t = spec.get_type();
    UtilityFunctions::print(String("SynergyRegistry: resource spec type for: ") + resource_path + String(" = ") + String::num(t));
    UtilityFunctions::print(spec);
    if (spec.get_type() == Variant::DICTIONARY) {
        // store synergies keyed by lowercase basename for predictable lookups
        register_synergy(key.to_lower(), spec);
        // Also register alias key based on declared component_aspects (if any)
        Dictionary sdict = spec;
        if (sdict.has("component_aspects")) {
            Variant cav = sdict["component_aspects"];
            if (cav.get_type() == Variant::ARRAY) {
                Array carr = cav;
                Array lower_aspects;
                for (int i = 0; i < carr.size(); ++i) {
                    Variant av = carr[i];
                    if (av.get_type() != Variant::STRING) continue;
                    String as = ((String)av).to_lower();
                    lower_aspects.push_back(as);
                }
                lower_aspects.sort();
                String alias = "";
                for (int i = 0; i < lower_aspects.size(); ++i) {
                    if (i) alias += "+";
                    alias += (String)lower_aspects[i];
                }
                if (alias != String() && !has_synergy(alias)) {
                    register_synergy(alias, spec);
                    UtilityFunctions::print(String("SynergyRegistry: registered alias key for resource '") + key + "' -> '" + alias + "'");
                }
            }
        }
        return true;
    }
    UtilityFunctions::print(String("SynergyRegistry: resource did not contain 'spec' dict: ") + resource_path);
    return false;
}

int SynergyRegistry::load_all_from_dir(const String &dir_path, bool recursive) {
    Ref<DirAccess> dir = DirAccess::open(dir_path);
    using namespace godot;
    if (!dir.is_valid()) {
        UtilityFunctions::print(String("SynergyRegistry: cannot open dir: ") + dir_path);
        return 0;
    }
    UtilityFunctions::print(String("SynergyRegistry: scanning dir: ") + dir_path);
    int registered = 0;
    dir->list_dir_begin();
    String fname = dir->get_next();
    while (fname != String()) {
        UtilityFunctions::print(String("SynergyRegistry: found entry: ") + fname + (dir->current_is_dir() ? " (dir)" : ""));
        if (dir->current_is_dir()) {
            if (recursive) {
                String subdir = dir_path + String("/") + fname;
                registered += load_all_from_dir(subdir, recursive);
            }
        } else {
            String lower = fname.to_lower();
            if (lower.ends_with(".json") || lower.ends_with(".tres") || lower.ends_with(".res")) {
                String key = fname.get_basename().to_lower();
                String full = dir_path + String("/") + fname;
                UtilityFunctions::print(String("SynergyRegistry: attempting to register from: ") + full + String(" as key=") + key);
                if (register_from_path(key, full)) {
                    registered++;
                    UtilityFunctions::print(String("SynergyRegistry: registered synergy: ") + key);
                } else {
                    UtilityFunctions::print(String("SynergyRegistry: failed to register synergy from: ") + full);
                }
            }
        }
        fname = dir->get_next();
    }
    dir->list_dir_end();
    return registered;
}
