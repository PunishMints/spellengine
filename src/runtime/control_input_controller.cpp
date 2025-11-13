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
    // Enable per-frame processing and input handling. Also ensure this node
    // continues to receive _process while the game may be paused by the
    // controller (we want input to remain responsive during control capture).
    set_process(true);
    set_process_input(true);
    // Diagnostic: indicate constructor ran and intended processing state
    // Note: leaving pause-mode handling to the caller/environment. We still
    // enable processing and input so hover updates occur while this node is
    // alive. If pause-mode control is required, adjust in the scene tree or
    // from the caller to ensure this node continues processing while paused.
}

ControlInputController::~ControlInputController() {
}

void ControlInputController::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_gizmo", "gizmo_node"), &ControlInputController::add_gizmo);
    ClassDB::bind_method(D_METHOD("remove_gizmo", "gizmo_node"), &ControlInputController::remove_gizmo);
    ClassDB::bind_method(D_METHOD("_restore_mouse_mode"), &ControlInputController::_restore_mouse_mode);
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
    UtilityFunctions::print(String("[ControlInputController] add_gizmo id=") + String::num_int64(gizmo_node->get_instance_id()) + String(" gizmo_count=") + String::num_int64(gizmo_list.size()));
    UtilityFunctions::print(String("[ControlInputController] state: is_processing=") + String(is_processing() ? "true" : "false") + String(" is_inside_tree=") + String(is_inside_tree() ? "true" : "false"));
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
                                // Defer restoring mouse mode and clearing the active-controls meta
                                // to the next idle/frame. This avoids races where the same input
                                // event that confirmed a control would immediately re-trigger
                                // game-level actions (e.g., starting a new spell) in the
                                // controller's _process. Use call_deferred so the current
                                // input event finishes processing first.
                                call_deferred("_restore_mouse_mode");
                                captured_mode_set = false;
                                prev_mouse_mode = -1;
                                // Clear active controls marker on parent via deferred call
                                Node *p = get_parent();
                                if (p) p->call_deferred("set_meta", String("spellengine_controls_active"), Variant(false));
                }
                UtilityFunctions::print(String("[ControlInputController] remove_gizmo id=") + String::num_int64(gizmo_node->get_instance_id()) + String(" gizmo_count=") + String::num_int64(gizmo_list.size()));
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

    // Live hover update: if we have a choose_vector gizmo with a single
    // prepopulated start point, update the second point on mouse motion so
    // the user sees a live preview of the final vector. This does not enable
    // click+drag; confirmation still requires an explicit click.
    if (mm && gizmo_list.size() > 0) {
        Variant v = gizmo_list[gizmo_list.size() - 1];
        if (v.get_type() == Variant::OBJECT) {
            Object *o = v;
            ControlGizmo *g = Object::cast_to<ControlGizmo>(o);
            if (g) {
                String mode = g->get_mode();
                Array existing = g->get_points();
                if ((mode.begins_with("choose_vector") || mode == "vector") && existing.size() == 1) {
                    // compute world point from screen using camera raycast where possible
                    Vector2 pos = mm->get_position();
                    Vector3 world_pt;
                    Camera3D *use_cam = camera;
                    if (!use_cam) {
                        Viewport *vp = get_viewport();
                        if (vp) use_cam = vp->get_camera_3d();
                    }
                    if (use_cam) {
                        Vector3 origin = use_cam->project_ray_origin(pos);
                        Vector3 dir = use_cam->project_ray_normal(pos);
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
                        // Debug: print mouse/camera/world point info so we can trace whether
                        // mouse motions are being observed and the raycast produced a hit.
                        UtilityFunctions::print(String("[ControlInputController] mouse_motion pos=") + Variant(pos).operator String());
                        if (use_cam) {
                            UtilityFunctions::print(String("[ControlInputController] using camera id=") + String::num_int64(use_cam->get_instance_id()));
                            UtilityFunctions::print(String("[ControlInputController] ray_origin=") + Variant(use_cam->project_ray_origin(pos)).operator String() + String(" dir=") + Variant(use_cam->project_ray_normal(pos)).operator String());
                        }
                        UtilityFunctions::print(String("[ControlInputController] computed world_pt=") + Variant(world_pt).operator String());
                    Array pnts; pnts.append(existing[0]); pnts.append(world_pt);
                    g->set_points(pnts);
                    // consume the mouse motion event while controls are active
                    Viewport *vp_final = get_viewport();
                    if (vp_final) vp_final->set_input_as_handled();
                    return;
                }
            }
        }
    }

    if (mb) {
        bool pressed = mb->is_pressed();
        int btn = mb->get_button_index();
        Vector2 pos = mb->get_position();

        const int LEFT = 1;
        if (pressed && btn == LEFT) {
            // Left-press semantics: for choose_vector-style gizmos we do NOT
            // support click+drag. Instead we accept distinct point selection:
            //  - If the gizmo has no points, the click sets the first point.
            //  - If the gizmo has one point, the click sets the second point and confirms.
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

                        String mode = g->get_mode();
                        Array existing = g->get_points();
                        if (mode.begins_with("choose_vector") || mode == "vector") {
                            // Distinct-click behavior
                            if (existing.size() == 0) {
                                Array pnts; pnts.append(world_pt); g->set_points(pnts);
                            } else {
                                // set second point and confirm
                                Array pnts; pnts.append(existing[0]); pnts.append(world_pt);
                                g->set_points(pnts);
                                g->confirm();
                            }
                            // Do not start drag behavior for choose_vector
                            return;
                        } else {
                            // Fallback to legacy click+drag for other gizmo modes
                            dragging = true;
                            drag_start_screen = pos;
                            drag_start_world = world_pt;
                            active_gizmo_obj = o;
                            Array pnts; pnts.append(world_pt); pnts.append(world_pt); g->set_points(pnts);
                            return;
                        }
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

void ControlInputController::_process(double delta) {
    // Diagnostic: indicate that _process was entered
    // Per-frame fallback for hover update: if no mouse motion events are
    // arriving (e.g., mouse is captured), still update the choose_vector
    // gizmo's second point to follow the current mouse/camera raycast.
    if (gizmo_list.size() == 0) return;
    Variant v = gizmo_list[gizmo_list.size() - 1];
    if (v.get_type() != Variant::OBJECT) return;
    Object *o = v;
    ControlGizmo *g = Object::cast_to<ControlGizmo>(o);
    if (!g) return;
    String mode = g->get_mode();
    Array existing = g->get_points();
    if (!(mode.begins_with("choose_vector") || mode == "vector")) return;
    if (existing.size() != 1) return;

    // compute world point from screen using camera raycast where possible
    Vector2 pos = get_viewport()->get_mouse_position();
    Vector3 world_pt;
    Camera3D *use_cam = camera;
    if (!use_cam) {
        Viewport *vp = get_viewport();
        if (vp) use_cam = vp->get_camera_3d();
    }
    if (use_cam) {
        Vector3 origin = use_cam->project_ray_origin(pos);
        Vector3 dir = use_cam->project_ray_normal(pos);
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

    // Debug: print per-frame mouse and raycast info so we can see whether
    // _process is computing new world points when no mouse-motion events fire.
    UtilityFunctions::print(String("[ControlInputController::_process] mouse_pos=") + Variant(pos).operator String());
    if (use_cam) {
        UtilityFunctions::print(String("[ControlInputController::_process] cam_id=") + String::num_int64(use_cam->get_instance_id()));
        UtilityFunctions::print(String("[ControlInputController::_process] ray_origin=") + Variant(use_cam->project_ray_origin(pos)).operator String());
    }
    UtilityFunctions::print(String("[ControlInputController::_process] computed world_pt=") + Variant(world_pt).operator String());

    Array pnts; pnts.append(existing[0]); pnts.append(world_pt);
    g->set_points(pnts);
}

void ControlInputController::set_camera(Camera3D *cam) {
    camera = cam;
}

int ControlInputController::get_gizmo_count() const {
    return gizmo_list.size();
}

void ControlInputController::_restore_mouse_mode() {
    Input *inp = Input::get_singleton();
    if (inp && captured_mode_set && prev_mouse_mode >= 0) {
        inp->set_mouse_mode((Input::MouseMode)prev_mouse_mode);
    }
    // Clear our flags; the deferred call ensures this runs after the input
    // event that confirmed a control has been fully processed.
    captured_mode_set = false;
    prev_mouse_mode = -1;
}
