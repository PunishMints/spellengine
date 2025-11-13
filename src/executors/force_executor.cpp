#include "spellengine/force_executor.hpp"

#include "spellengine/executor_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

using namespace godot;

void ForceExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    // Read context results early (we'll use chosen_position for directional impulses)
    Dictionary results = ctx->get_results();

    // Determine force vector: prefer a control-provided vector in the SpellContext results,
    // then resolved params, otherwise hardcode (0,5,0)
    Vector3 force_vec = Vector3(0, 5, 0);
    if (results.has("chosen_vector")) {
        Variant cv = results["chosen_vector"];
        if (cv.get_type() == Variant::VECTOR3) force_vec = cv;
    }

    if (force_vec == Vector3(0, 500, 0) && resolved_params.has("force")) {
        Variant fv = resolved_params["force"];
        if (fv.get_type() == Variant::VECTOR3) {
            force_vec = fv;
        } else if (fv.get_type() == Variant::ARRAY) {
            Array a = fv;
            double x = 0.0, y = 0.0, z = 0.0;
            if (a.size() >= 1) x = (double)a[0];
            if (a.size() >= 2) y = (double)a[1];
            if (a.size() >= 3) z = (double)a[2];
            force_vec = Vector3((float)x, (float)y, (float)z);
        }
    }

    // Prefer spawned_instances array if present (supports multi-spawn patterns),
    // otherwise fall back to last_spawned or ctx->get_targets().
    Array targets;
    if (results.has("spawned_instances") && results["spawned_instances"].get_type() == Variant::ARRAY) {
        targets = results["spawned_instances"];
    } else if (results.has("last_spawned") && results["last_spawned"].get_type() == Variant::OBJECT) {
        Object *last = Object::cast_to<Object>(results["last_spawned"]);
        if (last) targets.append(Variant(last));
    } else {
        targets = ctx->get_targets();
    }

    UtilityFunctions::print(String("ForceExecutor: applying force to targets_count=") + String::num(targets.size()));
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        if (v.get_type() != Variant::OBJECT) {
            UtilityFunctions::print(String("ForceExecutor: target[") + String::num(i) + "] is not an object; skipping");
            continue;
        }
        Object *obj = Object::cast_to<Object>(v);
        // Guard: ensure the object pointer is non-null and that the engine still
        // considers the instance id valid. Use UtilityFunctions::is_instance_id_valid
        // which accepts the instance id.
        if (!obj) {
            UtilityFunctions::print(String("ForceExecutor: target[") + String::num(i) + "] object cast failed; skipping");
            continue;
        }
        int64_t obj_iid = obj->get_instance_id();
        if (!UtilityFunctions::is_instance_id_valid(obj_iid)) {
            UtilityFunctions::print(String("ForceExecutor: target[") + String::num(i) + "] instance id invalid; skipping");
            continue;
        }
        Node *node = Object::cast_to<Node>(obj);
        String tname = String("<unknown>");
        if (node) tname = node->get_name();
        if (obj) tname = obj->get_class();
        UtilityFunctions::print(String("ForceExecutor: target[") + String::num(i) + "]: name=" + tname);

        if (node) {
            // If the node is flagged inert (spawned but not yet activated),
            // activate it now by clearing the meta flag and re-enabling physics
            // simulation. For RigidBody3D we restore the saved mode and wake it.
            if (node->has_meta(String("spell_inert"))) {
                Variant mi = node->get_meta(String("spell_inert"));
                if (mi.get_type() == Variant::BOOL && (bool)mi) {
                    UtilityFunctions::print(String("ForceExecutor: activating inert target '") + tname + "'");
                    node->set_meta(String("spell_inert"), Variant(false));
                    // If RigidBody3D, restore previous mode if present
                    RigidBody3D *rb_restore = Object::cast_to<RigidBody3D>(node);
                    if (rb_restore) {
                        Variant prev_mode_v = node->get_meta(String("spell_prev_mode"));
                        if (prev_mode_v.get_type() == Variant::Type::INT) {
                            int pm = (int)prev_mode_v;
                            rb_restore->set("mode", Variant(pm));
                        }
                        rb_restore->set("freeze", Variant(false));
                        // clear saved prev mode meta
                        if (node->has_meta(String("spell_prev_mode"))) node->set_meta(String("spell_prev_mode"), Variant());
                    } else {
                        node->set_physics_process(true);
                    }
                }
            }

            // Use standard native physics API for Node3D targets. Prefer casting
            // to RigidBody3D and calling the native apply_central_impulse binding.
            RigidBody3D *rb = Object::cast_to<RigidBody3D>(node);
            if (rb) {
                int64_t rb_iid = rb->get_instance_id();
                if (!UtilityFunctions::is_instance_id_valid(rb_iid)) {
                    UtilityFunctions::print(String("ForceExecutor: RigidBody3D target instance invalid; skipping"));
                    continue;
                }

                // If we have a chosen_position, compute a directional impulse from
                // the spawn point (node global origin) toward that position. Otherwise
                // fall back to applying the precomputed force_vec directly.
                if (results.has("chosen_position") && results["chosen_position"].get_type() == Variant::VECTOR3) {
                    Vector3 endpos = results["chosen_position"];
                    Vector3 spawn_pos = Vector3(0,0,0);
                    Node3D *n3node = Object::cast_to<Node3D>(node);
                    if (n3node) spawn_pos = n3node->get_global_transform().origin;
                    Vector3 dir = endpos - spawn_pos;
                    if (dir.length() <= 0.0001) {
                        UtilityFunctions::print(String("ForceExecutor: zero-length direction to chosen_position; skipping impulse for '") + node->get_name() + String("'"));
                        continue;
                    }
                    // Use magnitude from configured force_vec (length) if non-zero
                    double magnitude = (double)force_vec.length();
                    if (magnitude <= 0.0001) magnitude = 5.0;
                    Vector3 apply = dir.normalized() * (real_t)magnitude;
                    UtilityFunctions::print(String("ForceExecutor: applying directional impulse to '") + node->get_name() + String("' -> ") + Variant(apply).operator String());
                    rb->set("freeze", Variant(false));
                    rb->apply_central_impulse(apply);
                    continue;
                }

                UtilityFunctions::print(String("ForceExecutor: applying central impulse (native) to RigidBody3D '") + node->get_name() + String("' -> ") + Variant(force_vec).operator String());
                rb->set("freeze", Variant(false));
                rb->apply_central_impulse(force_vec);
                continue;
            }

            UtilityFunctions::print(String("ForceExecutor: target node is not a valid RigidBody3D; skipping physics impulse: ") + node->get_name());
        } else if (obj) {
            // If it's an Object that can be cast to RigidBody3D, prefer native call
            RigidBody3D *rb_obj = Object::cast_to<RigidBody3D>(obj);
            if (rb_obj) {
                int64_t rbobj_iid = rb_obj->get_instance_id();
                if (!UtilityFunctions::is_instance_id_valid(rbobj_iid)) {
                    UtilityFunctions::print(String("ForceExecutor: RigidBody3D object target instance invalid; skipping"));
                    continue;
                }
                UtilityFunctions::print(String("ForceExecutor: applying central impulse (native) to RigidBody3D object -> ") + Variant(force_vec).operator String());
                rb_obj->apply_central_impulse(force_vec);
            } else {
                UtilityFunctions::print(String("ForceExecutor: object target is not a valid RigidBody3D; skipping: ") + obj->get_class());
            }
        }
    }
}

void ForceExecutor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_param_schema"), &ForceExecutor::get_param_schema);
}

String ForceExecutor::get_executor_id() const {
    return String("force_v1");
}

Dictionary ForceExecutor::get_param_schema() const {
    Dictionary schema;
    Dictionary e;
    e = Dictionary();
    e["type"] = "array";
    Array def; def.append(0.0); def.append(5.0); def.append(0.0);
    e["default"] = def;
    e["desc"] = String("Force vector [x,y,z] to apply to the target");
    schema["force"] = e;
    return schema;
}

// Register factory for automatic registration at module init
REGISTER_EXECUTOR_FACTORY(ForceExecutor)
