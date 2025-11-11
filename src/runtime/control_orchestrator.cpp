#include "spellengine/control_orchestrator.hpp"
#include "spellengine/spell_engine.hpp"
#include "spellengine/control_manager.hpp"
// control_gizmo forward-referenced via ControlManager; header not required here
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

ControlOrchestrator::ControlOrchestrator() {}
ControlOrchestrator::~ControlOrchestrator() {}

void ControlOrchestrator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("resolve_controls", "spell", "ctx", "parent", "on_complete"), &ControlOrchestrator::resolve_controls);
    ClassDB::bind_method(D_METHOD("_on_single_control_done", "out", "index", "on_complete"), &ControlOrchestrator::_on_single_control_done);
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

    // All controls resolved: execute the spell and call final completion
    SpellEngine *se = memnew(SpellEngine());
    se->execute_spell(spell_ref, ctx_ref);
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

    Node *gizmo = cm->create_gizmo(exec_id);
    // Bind index then on_complete so our handler receives (out, index, on_complete)
    Callable user_cb = Callable(this, "_on_single_control_done").bind(Variant(idx), Variant(on_complete));
    // Attach and start, giving the parent and letting the manager know the preferred camera
    cm->ensure_input_controller(parent_node, cam);
    cm->attach_and_start(parent_node, gizmo, user_cb, exec_id);
}
