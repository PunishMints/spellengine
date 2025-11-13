// ControlOrchestrator: orchestrates pre-execution control resolution
#ifndef SPELLENGINE_CONTROL_ORCHESTRATOR_HPP
#define SPELLENGINE_CONTROL_ORCHESTRATOR_HPP

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/node.hpp>
#include "spellengine/spell.hpp"
#include "spellengine/spell_context.hpp"

using namespace godot;

class ControlManager;

class ControlOrchestrator : public Object {
    GDCLASS(ControlOrchestrator, Object);

protected:
    static void _bind_methods();

public:
    ControlOrchestrator();
    ~ControlOrchestrator();

    // Resolve controls for a spell in-order. `parent` is the node under which gizmos
    // will be attached (usually the current scene root). `on_complete` is called with
    // a Dictionary: {"ok":bool, "context":SpellContext}
    void resolve_controls(Variant spell, Variant ctx, Node *parent, const Callable &on_complete);

    // Internal callback when a single control completes. Not part of public API.
    void _on_single_control_done(const Variant &out, const Variant &index_v, const Callable &on_complete);

    // Deferred self-cleanup to allow safe deletion after callbacks return
    void _cleanup_self();

    // Set the first control index so the orchestrator can execute remaining
    // components after controls are resolved.
    void set_first_control_index(int idx);

private:
    // Stored state while resolving controls
    Array controls;
    Ref<Spell> spell_ref;
    Ref<SpellContext> ctx_ref;
    ControlManager *cm = nullptr;
    Node *parent_node = nullptr;
    Callable final_on_complete;
    int current_index = 0;
    // Index of the first control component in the spell's component list
    int first_control_index = 0;

    

    // Helper to start a control at the given index
    void _start_control_at(int idx, const Callable &on_complete);
};

#endif // SPELLENGINE_CONTROL_ORCHESTRATOR_HPP
