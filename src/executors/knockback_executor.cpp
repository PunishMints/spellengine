#include "spellengine/knockback_executor.hpp"


#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void KnockbackExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    Dictionary params = resolved_params;
    double force = 400.0;
    double speed = 500.0;
    if (params.has("force")) force = (double)params["force"];
    if (params.has("speed")) speed = (double)params["speed"];

    Node *caster = ctx->get_caster();

    Array targets = ctx->get_targets();
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        Node *node = Object::cast_to<Node>(v);
        if (!node) continue;

        if (node->has_method("apply_knockback")) {
            node->call("apply_knockback", Variant(force), Variant(speed), Variant(caster));
            continue;
        }

        UtilityFunctions::print(String("KnockbackExecutor: target lacks apply_knockback: ") + node->get_name());
    }
}

void KnockbackExecutor::_bind_methods() {}
