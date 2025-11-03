#include "spellengine/aspect_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/dir_access.hpp>

using namespace godot;

using namespace godot;

AspectRegistry *AspectRegistry::singleton = nullptr;

AspectRegistry *AspectRegistry::get_singleton() {
    if (!singleton) {
        singleton = memnew(AspectRegistry);
    }
    return singleton;
}

void AspectRegistry::register_aspect(const String &id, Ref<Aspect> aspect) {
    if (!has_aspect(id) && aspect.is_valid()) {
        aspects[id] = aspect;
    }
}

void AspectRegistry::unregister_aspect(const String &id) {
    if (has_aspect(id)) {
        aspects.erase(id);
    }
}

Array AspectRegistry::get_aspect_ids() const {
    return aspects.keys();
}

bool AspectRegistry::has_aspect(const String &id) const {
    return aspects.has(id);
}

Ref<Aspect> AspectRegistry::get_aspect(const String &id) const {
    if (!has_aspect(id)) return Ref<Aspect>();
    return aspects[id];
}

void AspectRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_aspect", "id", "aspect"), &AspectRegistry::register_aspect);
    ClassDB::bind_method(D_METHOD("unregister_aspect", "id"), &AspectRegistry::unregister_aspect);
    ClassDB::bind_method(D_METHOD("get_aspect_ids"), &AspectRegistry::get_aspect_ids);
    ClassDB::bind_method(D_METHOD("has_aspect", "id"), &AspectRegistry::has_aspect);
    ClassDB::bind_method(D_METHOD("get_aspect", "id"), &AspectRegistry::get_aspect);

    ClassDB::bind_method(D_METHOD("register_from_path", "id", "resource_path"), &AspectRegistry::register_from_path);
    ClassDB::bind_method(D_METHOD("load_all_from_dir", "dir_path", "recursive"), &AspectRegistry::load_all_from_dir);

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "aspect_ids"), "", "get_aspect_ids");
}

bool AspectRegistry::register_from_path(const String &id, const String &resource_path) {
    Ref<Resource> r = ResourceLoader::get_singleton()->load(resource_path);
    if (!r.is_valid()) return false;

    Ref<Aspect> a = Ref<Aspect>(r);
    if (!a.is_valid()) return false;

    register_aspect(id, a);
    return true;
}

int AspectRegistry::load_all_from_dir(const String &dir_path, bool recursive) {
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
                // derive id from filename (basename without extension)
                String id = fname.get_basename();
                String full = dir_path + String("/") + fname;
                if (register_from_path(id, full)) {
                    registered++;
                }
            }
        }
        fname = dir->get_next();
    }
    dir->list_dir_end();
    return registered;
}
