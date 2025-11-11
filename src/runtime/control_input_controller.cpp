#include "spellengine/control_input_controller.hpp"
#include "spellengine/control_gizmo.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>

using namespace godot;

ControlInputController::ControlInputController() {
}

ControlInputController::~ControlInputController() {
}

void ControlInputController::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_gizmo", "gizmo_node"), &ControlInputController::add_gizmo);
    ClassDB::bind_method(D_METHOD("remove_gizmo", "gizmo_node"), &ControlInputController::remove_gizmo);
    // _input overrides Node::_input; do not register it here to avoid duplicate
    // registration with ClassDB — the Node callback will dispatch to this override.
}

void ControlInputController::add_gizmo(Node *gizmo_node) {
    if (!gizmo_node) return;
    // If this is the first gizmo, enable UI-focused cursor mode and capture state
    Viewport *vp = get_viewport();
    if (gizmo_list.size() == 0) {
        // try to save previous mouse mode (if getter exists) and set visible
        Input *inp = Input::get_singleton();
        if (inp) {
            prev_mouse_mode = inp->get_mouse_mode();
            inp->set_mouse_mode(Input::MouseMode::MOUSE_MODE_VISIBLE);
            captured_mode_set = true;
        }
        // Mark parent node as having active controls so external logic (e.g., controller)
        // can avoid re-capturing the mouse or starting new spells while controls are active.
        Node *p = get_parent();
        if (p) p->set_meta(String("spellengine_controls_active"), Variant(true));
    }
    gizmo_list.append(Variant(gizmo_node));
}

void ControlInputController::remove_gizmo(Node *gizmo_node) {
    if (!gizmo_node) return;
    for (int i = 0; i < gizmo_list.size(); ++i) {
        Variant v = gizmo_list[i];
        if (v.get_type() == Variant::OBJECT) {
            Object *o = v;
            if (o == gizmo_node) {
                gizmo_list.remove_at(i);
                // If that was the last gizmo, restore previous cursor mode
                if (gizmo_list.size() == 0) {
                    Input *inp = Input::get_singleton();
                    if (inp && captured_mode_set && prev_mouse_mode >= 0) {
                        inp->set_mouse_mode((Input::MouseMode)prev_mouse_mode);
                    }
                    captured_mode_set = false;
                    prev_mouse_mode = -1;
                    // Clear active controls marker on parent so external logic can resume normal input
                    Node *p = get_parent();
                    if (p) p->set_meta(String("spellengine_controls_active"), Variant(false));
                }
                return;
            }
        }
    }
}

void ControlInputController::_input(const Ref<InputEvent> &event) {
    // Get camera from viewport if available
    // Use explicit camera if set; otherwise fall back to viewport camera
    Camera3D *cam = camera;
    
        // If any gizmo is active, pre-mark the event as handled so other systems
        // won't react while controls are active.
        Viewport *vp_pre = get_viewport();
        if (vp_pre && gizmo_list.size() > 0) {
            vp_pre->set_input_as_handled();
        }
    if (!cam) {
        Viewport *vp = get_viewport();
        if (vp) cam = vp->get_camera_3d();
    }

    // Try mouse button event handling for click/drag semantics
    InputEventMouseButton *mb = Object::cast_to<InputEventMouseButton>(event.ptr());
    InputEventMouseMotion *mm = Object::cast_to<InputEventMouseMotion>(event.ptr());

    if (mb) {
        bool pressed = mb->is_pressed();
        int btn = mb->get_button_index();
        Vector2 pos = mb->get_position();

        const int LEFT = 1;
        if (pressed && btn == LEFT) {
            // Start a drag with the top-most gizmo (last registered)
            if (gizmo_list.size() > 0) {
                Variant v = gizmo_list[gizmo_list.size() - 1];
                if (v.get_type() == Variant::OBJECT) {
                    Object *o = v;
                    ControlGizmo *g = Object::cast_to<ControlGizmo>(o);
                    if (g) {
                        // compute world point from screen using physics raycast where possible
                        Vector3 world_pt;
                        if (cam) {
                            Vector3 origin = cam->project_ray_origin(pos);
                            Vector3 dir = cam->project_ray_normal(pos);
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
                        dragging = true;
                        drag_start_screen = pos;
                        drag_start_world = world_pt;
                        active_gizmo_obj = o;
                        // initialize two-point array for drag-tracking
                        Array pnts;
                        pnts.append(world_pt);
                        pnts.append(world_pt);
                        g->set_points(pnts);
                    }
                }
            }
        } else if (!pressed && btn == LEFT) {
            // End drag / click. Confirm on active gizmo
            if (dragging && active_gizmo_obj) {
                ControlGizmo *g = Object::cast_to<ControlGizmo>(active_gizmo_obj);
                if (g) g->confirm();
            }
            dragging = false;
            active_gizmo_obj = nullptr;
        }
        return;
    }

    if (mm && dragging && active_gizmo_obj) {
        Vector2 pos = mm->get_position();
        Camera3D *use_cam = cam;
        Vector3 current_world;
        if (use_cam) {
            Vector3 origin = use_cam->project_ray_origin(pos);
            Vector3 dir = use_cam->project_ray_normal(pos);
            // Raycast to scene geometry for the current point
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
            current_world = candidate;
        } else {
            current_world = Vector3(pos.x, pos.y, 0);
        }
        ControlGizmo *g = Object::cast_to<ControlGizmo>(active_gizmo_obj);
        if (g) {
            Array pnts;
            pnts.append(drag_start_world);
            pnts.append(current_world);
            g->set_points(pnts);
        }
        return;
    }

    // Fallback: forward to all gizmos for event types we don't specifically handle
    for (int i = 0; i < gizmo_list.size(); ++i) {
        Variant v = gizmo_list[i];
        if (v.get_type() == Variant::OBJECT) {
            Object *o = v;
            ControlGizmo *g = Object::cast_to<ControlGizmo>(o);
        // If controls are active, make sure the event is treated as handled by the viewport
        Viewport *vp_final = get_viewport();
        if (vp_final && gizmo_list.size() > 0) vp_final->set_input_as_handled();
            if (g) {
                g->handle_input(event, cam);
            }
        }
    }
}

void ControlInputController::set_camera(Camera3D *cam) {
    camera = cam;
}
