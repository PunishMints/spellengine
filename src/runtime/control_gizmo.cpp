#include "spellengine/control_gizmo.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>

using namespace godot;

void ControlGizmo::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mode", "mode"), &ControlGizmo::set_mode);
    ClassDB::bind_method(D_METHOD("get_mode"), &ControlGizmo::get_mode);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "mode"), "set_mode", "get_mode");

    ClassDB::bind_method(D_METHOD("set_preview_scene", "scene"), &ControlGizmo::set_preview_scene);
    ClassDB::bind_method(D_METHOD("get_preview_scene"), &ControlGizmo::get_preview_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "preview_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_preview_scene", "get_preview_scene");

    ClassDB::bind_method(D_METHOD("add_point", "point"), &ControlGizmo::add_point);
    ClassDB::bind_method(D_METHOD("get_points"), &ControlGizmo::get_points);

    ClassDB::bind_method(D_METHOD("set_float_param", "key", "value"), &ControlGizmo::set_float_param);
    ClassDB::bind_method(D_METHOD("get_float_param", "key"), &ControlGizmo::get_float_param);

    ClassDB::bind_method(D_METHOD("set_completion_callable", "callable"), &ControlGizmo::set_completion_callable);
    ClassDB::bind_method(D_METHOD("confirm"), &ControlGizmo::confirm);
    ClassDB::bind_method(D_METHOD("cancel"), &ControlGizmo::cancel);
}

void ControlGizmo::set_mode(const String &m) {
    mode = m;
}

String ControlGizmo::get_mode() const {
    return mode;
}

void ControlGizmo::set_preview_scene(const Ref<PackedScene> &s) {
    preview_scene = s;
    // If an instance exists, free and recreate
    if (preview_instance) {
        preview_instance->queue_free();
        preview_instance = nullptr;
    }
    if (preview_scene.is_valid()) {
        Ref<PackedScene> ps = preview_scene;
        Node *inst = ps->instantiate();
        if (inst) {
            add_child(inst);
            preview_instance = inst;
        }
    }
}

void ControlGizmo::set_preview_instance(Node *inst) {
    if (preview_instance) {
        preview_instance->queue_free();
        preview_instance = nullptr;
    }
    if (inst) {
        add_child(inst);
        preview_instance = inst;
    }
}

Node *ControlGizmo::get_preview_instance() const {
    return preview_instance;
}

void ControlGizmo::handle_input(const Ref<InputEvent> &ev, Camera3D *camera) {
    // Minimal built-in input handling: for position modes, accept a left-click
    // and place a point at a camera ray projected point. This is intentionally
    // simple — a dedicated input controller will feed richer behavior.
    if (!ev.is_valid()) return;
    // Only handle mouse button presses for the prototype
    // Try casting to mouse button event for richer access
    InputEventMouseButton *mb = Object::cast_to<InputEventMouseButton>(ev.ptr());
    if (mb) {
        bool pressed = mb->is_pressed();
        int btn = mb->get_button_index();
        Vector2 pos = mb->get_position();

        const int LEFT = 1;
        if (pressed && btn == LEFT) {
            Vector3 world_pt;
            if (camera) {
                Vector3 origin = camera->project_ray_origin(pos);
                Vector3 dir = camera->project_ray_normal(pos);
                Viewport *vp = get_viewport();
                Vector3 candidate = origin + dir * 50.0;
                if (vp) {
                    Ref<World3D> w = vp->get_world_3d();
                        if (w.is_valid()) {
                            PhysicsDirectSpaceState3D *space = w->get_direct_space_state();
                            if (space) {
                                Array exclude;
                                double maxd = 1000.0;
                                Ref<PhysicsRayQueryParameters3D> rq = memnew(PhysicsRayQueryParameters3D());
                                rq->set_from(origin);
                                rq->set_to(origin + dir * maxd);
                                rq->set_exclude(exclude);
                                rq->set_collision_mask(0x7fffffff);
                                Variant hit = space->intersect_ray(rq);
                                if (hit.get_type() == Variant::DICTIONARY) {
                                    Dictionary d = hit;
                                    Variant posv = d.get(Variant("position"), Variant());
                                    if (posv.get_type() == Variant::VECTOR3) candidate = posv;
                                }
                            }
                        }
                }
                world_pt = candidate;
            } else {
                world_pt = Vector3(pos.x, pos.y, 0);
            }

            add_point(world_pt);
            if (mode.begins_with("choose_position") || mode == "position") {
                confirm();
            }
        }
    }
}

Ref<PackedScene> ControlGizmo::get_preview_scene() const {
    return preview_scene;
}

void ControlGizmo::add_point(const Variant &p) {
    // Accept Vector2 or Vector3 or arrays convertible to Vector3/Vector2
    points.append(p);
}

Array ControlGizmo::get_points() const {
    return points;
}

void ControlGizmo::set_points(const Array &pnts) {
    points = pnts;
}

void ControlGizmo::clear_points() {
    points.clear();
}

void ControlGizmo::set_float_param(const String &key, double v) {
    float_params[key] = v;
}

double ControlGizmo::get_float_param(const String &key) const {
    if (float_params.has(key)) {
        Variant v = float_params[key];
        if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT) return (double)v;
    }
    return 0.0;
}

void ControlGizmo::set_completion_callable(const Callable &c) {
    completion_callable = c;
}

void ControlGizmo::confirm() {
    // Build a canonical result dictionary depending on mode
    Dictionary res;
    if (mode.begins_with("choose_vector") || mode == "vector") {
        if (points.size() > 0) res["chosen_vector"] = points[0];
    } else if (mode.begins_with("choose_position") || mode == "position") {
        if (points.size() > 0) res["chosen_position"] = points[0];
    } else if (mode.begins_with("choose_polygon2d") || mode == "polygon2d") {
        res["chosen_polygon2d"] = points;
    } else if (mode.begins_with("choose_polygon3d") || mode == "polygon3d") {
        res["chosen_polygon3d"] = points;
        if (float_params.has("plane_normal")) res["plane_normal"] = float_params["plane_normal"];
    } else if (mode == "cylinder") {
        // expect center as first point, radius/height in float_params
        if (points.size() > 0) res["center"] = points[0];
        if (float_params.has("radius")) res["radius"] = float_params["radius"];
        if (float_params.has("height")) res["height"] = float_params["height"];
    } else {
        // generic fallback: provide points and float_params
        res["points"] = points;
        res["float_params"] = float_params;
    }

    // call completion callable if set
    if (completion_callable.is_valid()) {
        Array args;
        // Pass this gizmo as the first arg, then the result so the target
        // (ControlManager::_on_gizmo_complete) receives (gizmo_obj, result).
        args.append(Variant(this));
        args.append(res);
        completion_callable.callv(args);
    }

    // NOTE: temporarily avoid freeing the gizmo to help debug crash on
    // completion. This will leak the node until we implement a safe
    // lifetime cleanup strategy, but prevents use-after-free during
    // callback chains while we diagnose the issue.
    // queue_free();
}

void ControlGizmo::cancel() {
    if (completion_callable.is_valid()) {
        Array args;
        args.append(Variant(this));
        args.append(Dictionary());
        completion_callable.callv(args);
    }
    // See note in confirm(): avoid freeing for now while debugging
    // queue_free();
}
