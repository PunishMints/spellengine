// ControlManager: Orchestrates ControlGizmo instances, attaches them to scene tree,
// and forwards completion results to user callbacks after basic validation.
#ifndef SPELLENGINE_CONTROL_MANAGER_HPP
#define SPELLENGINE_CONTROL_MANAGER_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/camera3d.hpp>

using namespace godot;

class ControlManager : public Object {
    GDCLASS(ControlManager, Object);

private:
    // Map from gizmo instance id to user Callable
    Dictionary gizmo_user_callable;
    // Map from gizmo instance id to mode string
    Dictionary gizmo_mode_map;
         // Optional input controller owned by the manager (created on attach)
         class ControlInputController *input_controller = nullptr;
         // Optional preview factory for built-in previews
         class ControlPreviewFactory *preview_factory = nullptr;

protected:
    static void _bind_methods();

public:
    ControlManager();
    ~ControlManager();

    // Create a new ControlGizmo instance. Returns a Node* (the gizmo) which the caller
    // should add to the scene tree or pass to attach_and_start.
    Node *create_gizmo(const String &mode, const Ref<PackedScene> &preview_scene = Ref<PackedScene>());

    // Attach a gizmo to parent and start it. `user_completion` will be called with the
    // validated result (Dictionary) when the gizmo completes. `mode` should match the gizmo's mode.
    void attach_and_start(Node *parent, Node *gizmo_node, const Callable &user_completion, const String &mode = String());

    // Ensure an input controller exists under `parent` to route input to gizmos
    // Ensure an input controller exists under `parent` to route input to gizmos
    void ensure_input_controller(Node *parent, Camera3D *camera = nullptr);
    // Ensure preview factory exists (lazy)
    void ensure_preview_factory();

    // Internal callback from gizmo. Not intended to be called by users.
    // Note: signature expects gizmo_obj first because callables created via
    // Callable::bind prepend bound args. We bind the gizmo instance when
    // creating the callable so the bound gizmo will be passed as the first
    // argument; the gizmo will then call the callable with the result only.
    void _on_gizmo_complete(godot::Object *gizmo_obj, const godot::Variant &result);
};

#endif // SPELLENGINE_CONTROL_MANAGER_HPP
