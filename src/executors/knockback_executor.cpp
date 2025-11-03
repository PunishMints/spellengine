#include "spellengine/knockback_executor.hpp"


#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "spellengine/executor_registry.hpp"

using namespace godot;

void KnockbackExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    Dictionary params = resolved_params;
    double force = 400.0;
    double speed = 500.0;
    double area = 300.0;
    if (params.has("force")) force = (double)params["force"];
    if (params.has("speed")) speed = (double)params["speed"];
    if (params.has("area")) area = (double)params["area"];

    Node *caster = ctx->get_caster();

    Array targets = ctx->get_targets();
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        Node *node = Object::cast_to<Node>(v);
        if (!node) continue;

        // prepare metadata for optional metadata-capable targets
        Dictionary meta;
        meta["executor_id"] = get_executor_id();
        meta["phase"] = String("knockback");
        if (params.has("cast_id")) meta["cast_id"] = params["cast_id"];

        // Prefer calling an extended API if target supports metadata-aware knockback
        if (node->has_method("apply_knockback_meta")) {
            node->call("apply_knockback_meta", Variant(force), Variant(speed), Variant(area), Variant(caster), meta);
            continue;
        }

        if (node->has_method("apply_knockback")) {
            // fallback to the legacy 4-arg method
            node->call("apply_knockback", Variant(force), Variant(speed), Variant(area), Variant(caster));
            // also attempt to call a generic recorder if present so metadata isn't lost
            if (node->has_method("record_spell_event")) {
                node->call("record_spell_event", meta);
            }
            continue;
        }

        UtilityFunctions::print(String("KnockbackExecutor: target lacks apply_knockback: ") + node->get_name());
    }
}

void KnockbackExecutor::_bind_methods() {}

String KnockbackExecutor::get_executor_id() const {
    return String("knockback_v1");
}

// Register factory for automatic registration at module init
REGISTER_EXECUTOR_FACTORY(KnockbackExecutor)
