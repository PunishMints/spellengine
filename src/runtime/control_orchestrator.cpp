#include "spellengine/control_orchestrator.hpp"
#include "spellengine/spell_engine.hpp"
#include "spellengine/control_manager.hpp"
#include "spellengine/control_gizmo.hpp"
// control_gizmo forward-referenced via ControlManager; header not required here
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

ControlOrchestrator::ControlOrchestrator() {}
ControlOrchestrator::~ControlOrchestrator() {}

void ControlOrchestrator::set_first_control_index(int idx) {
    first_control_index = idx;
}

void ControlOrchestrator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("resolve_controls", "spell", "ctx", "parent", "on_complete"), &ControlOrchestrator::resolve_controls);
    ClassDB::bind_method(D_METHOD("_on_single_control_done", "out", "index", "on_complete"), &ControlOrchestrator::_on_single_control_done);
    ClassDB::bind_method(D_METHOD("set_first_control_index", "idx"), &ControlOrchestrator::set_first_control_index);
    ClassDB::bind_method(D_METHOD("_cleanup_self"), &ControlOrchestrator::_cleanup_self);
}

void ControlOrchestrator::resolve_controls(Variant spell_v, Variant ctx_v, Node *parent, const Callable &on_complete) {
    // Basic guards
    if (!spell_v.has_method("get")) {
        // Not a resource or invalid
        Array args;
        Dictionary out;
        out["ok"] = false;
        out["error"] = String("invalid_spell");
        args.append(out);
        on_complete.callv(args);
        return;
    }

    // Cast to expected types via Variant and store state
    if (spell_v.get_type() == Variant::OBJECT) {
        spell_ref = spell_v;
    }
    if (ctx_v.get_type() == Variant::OBJECT) {
        ctx_ref = ctx_v;
    }

    parent_node = parent;
    final_on_complete = on_complete;

    // Use SpellEngine::collect_controls to get list of control entries
    SpellEngine *se = memnew(SpellEngine());
    controls = se->collect_controls(spell_ref, ctx_ref);
    memdelete(se);

    // If no controls, immediately call on_complete with ok=true and unchanged context
    if (controls.size() == 0) {
        Array args;
        Dictionary out;
        out["ok"] = true;
        out["context"] = ctx_ref;
        args.append(out);
        final_on_complete.callv(args);
        return;
    }

    // Create a ControlManager to spawn gizmos (kept alive until orchestration completes)
    cm = memnew(ControlManager());
    current_index = 0;

    // Start first control
    _start_control_at(current_index, final_on_complete);
}

void ControlOrchestrator::_on_single_control_done(const Variant &out, const Variant &index_v, const Callable &on_complete) {
    UtilityFunctions::print(String("[ControlOrchestrator] _on_single_control_done called for index=") + String::num_int64((int64_t)index_v));
    // out is a Dictionary {"ok":bool, "result":Dictionary}
    int index = 0;
    if (index_v.get_type() == Variant::INT) {
        index = index_v;
    }

    Dictionary d = out;
    Variant ok_v = d.get(Variant("ok"), Variant());
    bool ok = false;
    if (ok_v.get_type() == Variant::Type::BOOL) ok = ok_v;

    // If validation failed, abort and forward failure
    if (!ok) {
        Array args;
        Dictionary outd;
        outd["ok"] = false;
        outd["error"] = String("control_validation_failed");
        outd["index"] = index;
        outd["result"] = d.get(Variant("result"), Variant());
        args.append(outd);
        // call the final on_complete (either bound or stored)
        if (on_complete.is_valid()) on_complete.callv(args);
        else if (final_on_complete.is_valid()) final_on_complete.callv(args);
        // Defer cleanup so we don't delete the ControlManager while its
        // methods are still on the call stack (it may be the caller of our
        // completion callback). Schedule deferred cleanup which will free
        // the ControlManager and then this orchestrator.
        UtilityFunctions::print("[ControlOrchestrator] validation failed; scheduling deferred cleanup (failure path)");
        call_deferred("_cleanup_self");
        return;
    }

    // Merge validated result into SpellContext results
    Variant res_v = d.get(Variant("result"), Variant());
    if (ctx_ref.is_valid() && res_v.get_type() == Variant::DICTIONARY) {
        Dictionary current = ctx_ref->get_results();
        Dictionary newr = res_v;
        Array keys = newr.keys();
        for (int i = 0; i < keys.size(); ++i) {
            Variant k = keys[i];
            current[k] = newr[k];
        }
        ctx_ref->set_results(current);
    }

    // Move to next control or finish
    int next = index + 1;
    if (next < controls.size()) {
        current_index = next;
        // start next control; bind the same final_on_complete for cascading
        _start_control_at(current_index, final_on_complete);
        return;
    }

    // All controls resolved: execute the remaining components (from first_control_index to end)
    SpellEngine *se = memnew(SpellEngine());
    int total_components = 0;
    if (spell_ref.is_valid()) {
        TypedArray<Ref<SpellComponent>> comps = spell_ref->get_components();
        total_components = comps.size();
    }
    se->execute_components_range(spell_ref, ctx_ref, first_control_index, total_components);
    memdelete(se);

    Array args;
    Dictionary outd;
    outd["ok"] = true;
    outd["context"] = ctx_ref;
    args.append(outd);
    if (on_complete.is_valid()) on_complete.callv(args);
    else if (final_on_complete.is_valid()) final_on_complete.callv(args);

    // Defer cleanup of the ControlManager and orchestrator so we don't
    // delete objects while their methods are still executing on the
    // current call stack (the completion callback above may have been
    // invoked from a ControlManager method).
    UtilityFunctions::print("[ControlOrchestrator] all controls resolved; scheduling deferred cleanup (success path)");
    call_deferred("_cleanup_self");
}

void ControlOrchestrator::_cleanup_self() {
    // Safe deferred deletion point
    UtilityFunctions::print("[ControlOrchestrator] _cleanup_self running");
    // Perform final cleanup: delete ControlManager and this orchestrator.
    if (cm) { memdelete(cm); cm = nullptr; }
    memdelete(this);
}

void ControlOrchestrator::_start_control_at(int idx, const Callable &on_complete) {
    if (idx < 0 || idx >= controls.size()) {
        Array args;
        Dictionary out;
        out["ok"] = false;
        out["error"] = String("index_out_of_range");
        args.append(out);
        if (on_complete.is_valid()) on_complete.callv(args);
        return;
    }

    Variant ctrl_v = controls[idx];
    if (ctrl_v.get_type() != Variant::DICTIONARY) {
        Array args;
        Dictionary out;
        out["ok"] = false;
        out["error"] = String("malformed_control_entry");
        args.append(out);
        if (on_complete.is_valid()) on_complete.callv(args);
        return;
    }

    Dictionary c = ctrl_v;
    Variant exec_id_v = c.get(Variant("executor_id"), Variant());
    String exec_id;
    if (exec_id_v.get_type() == Variant::Type::STRING) exec_id = exec_id_v;

    if (!cm) {
        cm = memnew(ControlManager());
    }
    // Determine a Camera3D to pass into the input controller if available.
    Camera3D *cam = nullptr;
    if (ctx_ref.is_valid()) {
        Node *caster = ctx_ref->get_caster();
        if (caster) {
            // try to find a Camera3D child on the caster
            Node *maybe_cam = caster->get_node_or_null(String("Camera3D"));
            if (maybe_cam) cam = Object::cast_to<Camera3D>(maybe_cam);
        }
    }
    // Precompute potential prepopulated points from base_params or context
    Array start_pts;
    Variant bp_v = c.get(Variant("base_params"), Variant());
    if (bp_v.get_type() == Variant::DICTIONARY) {
        Dictionary bp = bp_v;
        if (bp.has("points") && bp["points"].get_type() == Variant::ARRAY) {
            start_pts = bp["points"];
        } else {
            const Vector<String> keys = {"point","position","center","origin","projectile_position","spawn_position","spawn_point"};
            for (int ki = 0; ki < keys.size(); ++ki) {
                String k = keys[ki];
                if (bp.has(k)) {
                    Variant v = bp[k];
                    if (v.get_type() == Variant::VECTOR3) { Array a; a.append(v); start_pts = a; break; }
                }
            }
        }
    }

    if (start_pts.size() == 0 && ctx_ref.is_valid()) {
        Dictionary res = ctx_ref->get_results();
        // If multiple instances were spawned by a SummonExecutor, prefer the
        // average of their global positions as the prepopulated start point
        // so the gizmo preview anchors to the group's center.
        if (res.has("spawned_instances") && res["spawned_instances"].get_type() == Variant::ARRAY) {
            Array sa = res["spawned_instances"];
            Vector3 sum = Vector3(0,0,0);
            int found = 0;
            for (int si = 0; si < sa.size(); ++si) {
                Variant sv = sa[si];
                if (sv.get_type() != Variant::OBJECT) continue;
                Object *o = Object::cast_to<Object>(sv);
                if (!o) continue;
                if (!UtilityFunctions::is_instance_id_valid(o->get_instance_id())) continue;
                Node3D *n3 = Object::cast_to<Node3D>(o);
                if (!n3) continue;
                if (!n3->is_inside_tree()) continue;
                sum += n3->get_global_transform().origin;
                found += 1;
            }
            if (found > 0) {
                Vector3 avg = sum / (real_t)found;
                Array a; a.append(avg);
                start_pts = a;
                UtilityFunctions::print(String("[ControlOrchestrator] prepopulating gizmo from spawned_instances average at ") + Variant(avg).operator String());
            }
        }
        else if (res.has("last_spawned")) {
            Variant lt = res["last_spawned"];
            if (lt.get_type() == Variant::OBJECT) {
                Object *o = Object::cast_to<Object>(lt);
                if (o) {
                    Node3D *n3 = Object::cast_to<Node3D>(o);
                    if (n3) {
                        int64_t iid = n3->get_instance_id();
                        if (!UtilityFunctions::is_instance_id_valid(iid)) {
                            UtilityFunctions::print(String("[ControlOrchestrator] last_spawned present but instance id invalid; skipping prepopulate"));
                        } else if (!n3->is_inside_tree()) {
                            UtilityFunctions::print(String("[ControlOrchestrator] last_spawned Node3D exists but is not inside scene tree; skipping prepopulate"));
                        } else {
                            Vector3 pos = n3->get_global_transform().origin;
                            Array a; a.append(pos); start_pts = a;
                            UtilityFunctions::print(String("[ControlOrchestrator] prepopulating gizmo from last_spawned at ") + Variant(pos).operator String());
                        }
                    }
                }
            } else if (lt.get_type() == Variant::VECTOR3) {
                Array a; a.append(lt); start_pts = a;
            }
        }

        if (start_pts.size() == 0) {
            const Vector<String> res_keys = {"projectile_position","spawn_position","spawn_point","origin","position","point"};
            for (int ri = 0; ri < res_keys.size(); ++ri) {
                String rk = res_keys[ri];
                if (res.has(rk)) {
                    Variant rv = res[rk];
                    if (rv.get_type() == Variant::VECTOR3) { Array a; a.append(rv); start_pts = a; break; }
                }
            }
        }
    }

    // Determine how many points this control mode expects (simple heuristic)
    int expected_points = 1;
    if (exec_id.begins_with("choose_vector") || exec_id == "vector") expected_points = 2;

    // If we already have enough prepopulated points to satisfy the control,
    // skip creating/attaching a gizmo and treat the control as already
    // completed by synthesizing the control result and invoking the
    // orchestrator's single-control completion handler.
    Callable user_cb = Callable(this, "_on_single_control_done").bind(Variant(idx), Variant(on_complete));
    if (start_pts.size() >= expected_points) {
        // Build canonical result dictionary for this control mode
        Dictionary result;
        if (expected_points == 2) {
            Vector3 p0 = start_pts[0];
            Vector3 p1 = start_pts[1];
            result["chosen_vector"] = p1 - p0;
            result["chosen_position"] = p0;
        } else if (start_pts.size() >= 1) {
            result["chosen_position"] = start_pts[0];
        }

        // Validate using SpellEngine and then call the orchestrator handler
        SpellEngine *se = memnew(SpellEngine());
        bool ok = se->validate_control_result(exec_id, result);
        memdelete(se);

        Array args;
        Dictionary out;
        out["ok"] = ok;
        out["result"] = result;
        args.append(out);
        // Call the orchestrator's completion handler directly (it will
        // merge results and continue to the next control).
        if (user_cb.is_valid()) user_cb.callv(args);
        else UtilityFunctions::print(String("[ControlOrchestrator] user_cb invalid when trying to short-circuit prepopulated control"));

        return;
    }

    // Otherwise, create the gizmo and attach as normal, and if we have a
    // single start point, set that on the gizmo so the preview anchors to it.
    Node *gizmo = cm->create_gizmo(exec_id);
    // Bind index then on_complete so our handler receives (out, index, on_complete)
    // user_cb already created above
    // Attach and start, giving the parent and letting the manager know the preferred camera
    cm->ensure_input_controller(parent_node, cam);
    cm->attach_and_start(parent_node, gizmo, user_cb, exec_id);

    ControlGizmo *cg = Object::cast_to<ControlGizmo>(gizmo);
    if (cg && start_pts.size() > 0) {
        // Only set the provided points (1 point allowed) so that hover
        // updates and distinct-click confirmation behave correctly.
        Array pts_to_set;
        if (start_pts.size() >= 1) pts_to_set.append(start_pts[0]);
        cg->set_points(pts_to_set);
    }
}
