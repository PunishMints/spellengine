#include "spellengine/damage_executor.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void DamageExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    Dictionary params = resolved_params;
    double amount = 0.0;
    String element = "";
    if (params.has("amount")) amount = (double)params["amount"];
    if (params.has("element")) element = (String)params["element"];

    Array targets = ctx->get_targets();
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        Object *obj = Object::cast_to<Object>(v);
        Node *node = Object::cast_to<Node>(v);
        if (node) {
            if (node->has_method("apply_damage")) {
                node->call("apply_damage", Variant(amount), Variant(element));
            } else {
                UtilityFunctions::print(String("DamageExecutor: target has no apply_damage: ") + node->get_name());
            }
        } else if (obj) {
            if (obj->has_method("apply_damage")) obj->call("apply_damage", Variant(amount), Variant(element));
        }
    }
}

void DamageExecutor::_bind_methods() {
    // No additional bindings required for now
}
