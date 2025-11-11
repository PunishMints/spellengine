// ControlGizmo: runtime node that assists interactive control capture
#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/classes/packed_scene.hpp>

using namespace godot;

namespace godot {
class Camera3D;
class InputEvent;
}

#include <godot_cpp/classes/input_event.hpp>

class ControlGizmo : public Node3D {
    GDCLASS(ControlGizmo, Node3D)

protected:
    static void _bind_methods();

private:
    String mode = "vector"; // e.g., choose_vector, choose_position, choose_polygon2d, choose_polygon3d, cylinder
    Array points; // Array of Vector2/Vector3 depending on mode
    Dictionary float_params; // generic numeric params like radius/height
    Ref<PackedScene> preview_scene;
    Node *preview_instance = nullptr;
    Callable completion_callable;

public:
    void set_mode(const String &m);
    String get_mode() const;

    void set_preview_scene(const Ref<PackedScene> &s);
    Ref<PackedScene> get_preview_scene() const;

    // Allow a pre-created preview instance (Node3D) to be attached directly.
    void set_preview_instance(Node *inst);
    Node *get_preview_instance() const;

    // Programmatic helpers used by UI or gameplay code to feed points
    void add_point(const Variant &p);
    Array get_points() const;
    void set_points(const Array &pnts);
    void clear_points();

    // Handle input events routed from an input controller. Camera may be null
    // for screen-space inputs.
    void handle_input(const Ref<InputEvent> &ev, Camera3D *camera);

    void set_float_param(const String &key, double v);
    double get_float_param(const String &key) const;

    // Completion / cancellation
    void set_completion_callable(const Callable &c);
    void confirm();
    void cancel();
};
