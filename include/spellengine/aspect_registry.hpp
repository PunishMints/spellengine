// Registry for Aspect resources
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include "spellengine/aspect.hpp"

using namespace godot;

class AspectRegistry : public Object {
    GDCLASS(AspectRegistry, Object)

protected:
    static void _bind_methods();

private:
    // map aspect id -> Aspect resource
    Dictionary aspects;
    static AspectRegistry *singleton;

public:
    // Singleton accessor
    static AspectRegistry *get_singleton();

    void register_aspect(const String &id, Ref<Aspect> aspect);
    void unregister_aspect(const String &id);
    Array get_aspect_ids() const;
    bool has_aspect(const String &id) const;
    Ref<Aspect> get_aspect(const String &id) const;
    // Convenience: load a resource from a path and register it under `id`.
    bool register_from_path(const String &id, const String &resource_path);

    // Scan a directory for .tres/.res aspect resources and register them.
    // Returns the number of aspects successfully registered.
    int load_all_from_dir(const String &dir_path, bool recursive = true);
};
