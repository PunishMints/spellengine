#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

using namespace godot;

namespace godot { class Camera3D; }

class ControlInputController : public Node {
    GDCLASS(ControlInputController, Node);

protected:
    static void _bind_methods();

private:
    Array gizmo_list; // stores Node* (ControlGizmo instances)
    Camera3D *camera = nullptr;
    // Drag state
    bool dragging = false;
    Vector2 drag_start_screen;
    Vector3 drag_start_world;
    Object *active_gizmo_obj = nullptr; // raw pointer to gizmo object
    // Cursor / input capture state
    int prev_mouse_mode = -1;
    bool captured_mode_set = false;
    // Deferred restore helper
    void _restore_mouse_mode();

public:
    ControlInputController();
    ~ControlInputController();

    void add_gizmo(Node *gizmo_node);
    void remove_gizmo(Node *gizmo_node);

    // Query how many gizmos are currently registered
    int get_gizmo_count() const;

    // Input handler - called by engine
    void _input(const Ref<InputEvent> &event) override;
    void _process(double delta) override;

    // Set explicit camera to use for screen->world projection
    void set_camera(Camera3D *cam);
};
