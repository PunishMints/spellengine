#include "spellengine/spell_component_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/dir_access.hpp>

using namespace godot;

SpellComponentRegistry *SpellComponentRegistry::singleton = nullptr;

SpellComponentRegistry *SpellComponentRegistry::get_singleton() {
    if (!singleton) singleton = memnew(SpellComponentRegistry);
    return singleton;
}

void SpellComponentRegistry::register_component(const String &id, Ref<SpellComponent> comp) {
    if (id == String() || !comp.is_valid()) return;
    if (!has_component(id)) components[id] = comp;
}

void SpellComponentRegistry::unregister_component(const String &id) {
    if (components.has(id)) components.erase(id);
}

Array SpellComponentRegistry::get_component_ids() const {
    return components.keys();
}

bool SpellComponentRegistry::has_component(const String &id) const {
    return components.has(id);
}

Ref<SpellComponent> SpellComponentRegistry::get_component(const String &id) const {
    if (!has_component(id)) return Ref<SpellComponent>();
    return components[id];
}

void SpellComponentRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_component", "id", "component"), &SpellComponentRegistry::register_component);
    ClassDB::bind_method(D_METHOD("unregister_component", "id"), &SpellComponentRegistry::unregister_component);
    ClassDB::bind_method(D_METHOD("get_component_ids"), &SpellComponentRegistry::get_component_ids);
    ClassDB::bind_method(D_METHOD("has_component", "id"), &SpellComponentRegistry::has_component);
    ClassDB::bind_method(D_METHOD("get_component", "id"), &SpellComponentRegistry::get_component);

    ClassDB::bind_method(D_METHOD("register_from_path", "id", "resource_path"), &SpellComponentRegistry::register_from_path);
    ClassDB::bind_method(D_METHOD("load_all_from_dir", "dir_path", "recursive"), &SpellComponentRegistry::load_all_from_dir);

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "component_ids"), "", "get_component_ids");
}

bool SpellComponentRegistry::register_from_path(const String &id, const String &resource_path) {
    Ref<Resource> r = ResourceLoader::get_singleton()->load(resource_path);
    if (!r.is_valid()) return false;

    Ref<SpellComponent> sc = Ref<SpellComponent>(r);
    if (!sc.is_valid()) return false;

    register_component(id, sc);
    return true;
}

int SpellComponentRegistry::load_all_from_dir(const String &dir_path, bool recursive) {
    Ref<DirAccess> dir = DirAccess::open(dir_path);
    if (!dir.is_valid()) return 0;

    int registered = 0;
    dir->list_dir_begin();
    String fname = dir->get_next();
    while (fname != String()) {
        if (dir->current_is_dir()) {
            if (recursive) {
                String subdir = dir_path + String("/") + fname;
                registered += load_all_from_dir(subdir, recursive);
            }
        } else {
            String lower = fname.to_lower();
            if (lower.ends_with(".tres") || lower.ends_with(".res")) {
                String id = fname.get_basename();
                String full = dir_path + String("/") + fname;
                if (register_from_path(id, full)) registered++;
            }
        }
        fname = dir->get_next();
    }
    dir->list_dir_end();
    return registered;
}
