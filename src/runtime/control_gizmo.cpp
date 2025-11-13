#include "spellengine/control_gizmo.hpp"
#include "godot_cpp/classes/control.hpp"

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
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>

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

    // If this is a vector-mode gizmo, update the preview visualization
    if (mode.begins_with("choose_vector") || mode == "vector") {
        if (!preview_instance) {
            // create a simple root container under this gizmo to hold preview geometry
            Node3D *root = memnew(Node3D());
            add_child(root);
            preview_instance = root;
            preview_vector_root = root;
        }

        // Ensure there is a single consistent preview structure under preview_instance
        // with named children we can find and update. This avoids creating/removing
        // multiple transient preview nodes and lets us simply overwrite data.
        MeshInstance3D *mi = nullptr;
        MeshInstance3D *start_sphere = nullptr;
        MeshInstance3D *end_sphere = nullptr;

        // Try to find existing named nodes first (useful when a preview_scene
        // was provided by the preview factory). Names: "preview_line", "preview_start", "preview_end".
        if (preview_instance) {
            Node *nline = preview_instance->get_node_or_null(String("preview_line"));
            if (nline) mi = Object::cast_to<MeshInstance3D>(nline);
            Node *nstart = preview_instance->get_node_or_null(String("preview_start"));
            if (nstart) start_sphere = Object::cast_to<MeshInstance3D>(nstart);
            Node *nend = preview_instance->get_node_or_null(String("preview_end"));
            if (nend) end_sphere = Object::cast_to<MeshInstance3D>(nend);
        }

        // If members were already cached earlier, prefer them
        if (!mi && preview_vector_line) mi = Object::cast_to<MeshInstance3D>(preview_vector_line);
        if (!start_sphere && preview_vector_start) start_sphere = Object::cast_to<MeshInstance3D>(preview_vector_start);
        if (!end_sphere && preview_vector_end) end_sphere = Object::cast_to<MeshInstance3D>(preview_vector_end);

        // Create any missing nodes and attach them (name them for later lookup)
        if (!mi) {
            mi = memnew(MeshInstance3D());
            mi->set_name(String("preview_line"));
            preview_instance->add_child(mi);
            preview_vector_line = mi;
            Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D());
            mat->set_albedo(Color(0.2, 0.8, 0.4, 1.0));
            mat->set_emission(Color(0.4, 1.0, 0.6));
            mi->set_material_override(mat);
        } else {
            preview_vector_line = mi;
        }

        if (!start_sphere) {
            start_sphere = memnew(MeshInstance3D());
            start_sphere->set_name(String("preview_start"));
            Ref<SphereMesh> sm = memnew(SphereMesh());
            sm->set_radius(0.12);
            start_sphere->set_mesh(sm);
            Ref<StandardMaterial3D> smat = memnew(StandardMaterial3D());
            smat->set_albedo(Color(1.0, 0.4, 0.2, 1.0));
            start_sphere->set_material_override(smat);
            preview_instance->add_child(start_sphere);
            preview_vector_start = start_sphere;
        } else {
            preview_vector_start = start_sphere;
        }

        if (!end_sphere) {
            end_sphere = memnew(MeshInstance3D());
            end_sphere->set_name(String("preview_end"));
            Ref<SphereMesh> sm2 = memnew(SphereMesh());
            sm2->set_radius(0.08);
            end_sphere->set_mesh(sm2);
            Ref<StandardMaterial3D> emat = memnew(StandardMaterial3D());
            emat->set_albedo(Color(0.2, 0.6, 1.0, 1.0));
            end_sphere->set_material_override(emat);
            preview_instance->add_child(end_sphere);
            preview_vector_end = end_sphere;
        } else {
            preview_vector_end = end_sphere;
        }

    Vector3 p0 = Vector3(0,0,0);
    Vector3 p1 = Vector3(0,0,0);
        if (points.size() > 0) {
            Variant v0 = points[0];
            if (v0.get_type() == Variant::VECTOR3) p0 = v0;
        }
        if (points.size() > 1) {
            Variant v1 = points[1];
            if (v1.get_type() == Variant::VECTOR3) p1 = v1;
        }

    // Convert world-space points into this gizmo's local space so the
    // mesh vertices are positioned correctly relative to the MeshInstance3D.
        Transform3D inv = get_global_transform().affine_inverse();
        Vector3 lp0 = inv.xform(p0);
        Vector3 lp1 = inv.xform(p1);

    // Update preview geometry for the current points.

        if (points.size() > 1) {
            // Build an ArrayMesh with a single line segment
            Ref<ArrayMesh> am = memnew(ArrayMesh());
            Array arrays;
            arrays.resize(Mesh::ARRAY_MAX);
            PackedVector3Array verts;
            verts.append(lp0);
            verts.append(lp1);
            arrays[Mesh::ARRAY_VERTEX] = verts;
            am->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
            mi->set_mesh(am);

            // Ensure end marker is visible and positioned
            if (end_sphere) {
                end_sphere->set_transform(Transform3D(Basis(), lp1));
                end_sphere->set_visible(true);
            }
            if (start_sphere) {
                start_sphere->set_transform(Transform3D(Basis(), lp0));
                start_sphere->set_visible(true);
            }
        } else {
            // Only a single start point: show only the start marker and
            // clear/hide any line or endpoint marker to avoid showing a
            // misleading line to the origin or center of screen.
            if (mi) {
                mi->set_mesh(Ref<ArrayMesh>());
            }
            if (start_sphere) {
                start_sphere->set_transform(Transform3D(Basis(), lp0));
            }
            if (end_sphere) {
                end_sphere->set_visible(false);
            }
        }
    }

}

void ControlGizmo::_process(double delta) {
    // Diagnostic: confirm _process entry
    // Only run hover polling when the gizmo expects a second point and
    // currently has exactly one start point.
    if (!(mode.begins_with("choose_vector") || mode == "vector")) return;
    //if (points.size() != 1) return;

    Vector3 p0 = Vector3();
    Variant v0 = points[0];
    if (v0.get_type() == Variant::VECTOR3) p0 = v0;

    Viewport *vp = get_viewport();
    if (!vp) return;
    Camera3D *cam = vp->get_camera_3d();
    Vector2 mpos = vp->get_mouse_position();
    Vector3 world_pt;
    if (cam) {
        Vector3 origin = cam->project_ray_origin(mpos);
        Vector3 dir = cam->project_ray_normal(mpos);
        Vector3 candidate = origin + dir * 50.0;
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
        world_pt = candidate;
    } else {
        world_pt = Vector3(mpos.x, mpos.y, 0);
    }

    // Update hover point each frame
    Array pnts; pnts.append(p0); pnts.append(world_pt);
    set_points(pnts);
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
        // For vector selection we expect two points (start, end). Prefer
        // returning the direction vector (end - start). If only a single
        // point is present, fall back to that point (legacy behavior).
        if (points.size() > 1) {
            Variant v0 = points[0];
            Variant v1 = points[1];
            if (v0.get_type() == Variant::VECTOR3 && v1.get_type() == Variant::VECTOR3) {
                Vector3 p0 = v0;
                Vector3 p1 = v1;
                res["chosen_vector"] = p1 - p0;
            } else if (v1.get_type() == Variant::VECTOR3) {
                res["chosen_vector"] = v1;
            } else if (v0.get_type() == Variant::VECTOR3) {
                res["chosen_vector"] = v0;
            }
        } else if (points.size() > 0) {
            res["chosen_vector"] = points[0];
        }
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
        // Print the final selection for debugging/confirmation
        UtilityFunctions::print(String("[ControlGizmo] confirm result: ") + Variant(res).operator String());
        completion_callable.callv(args);
    }
    // Gizmo lifetime is managed by the ControlManager; do not free here.
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
