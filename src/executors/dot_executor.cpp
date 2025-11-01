#include "spellengine/dot_executor.hpp"
#include "spellengine/status_effect.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void DotExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    Dictionary params = resolved_params;
    double amount = 0.0;
    double interval = 1.0;
    double duration = 5.0;
    String element = "";

    if (params.has("amount_per_tick")) amount = (double)params["amount_per_tick"];
    if (params.has("tick_interval")) interval = (double)params["tick_interval"];
    if (params.has("duration")) duration = (double)params["duration"];
    if (params.has("element")) element = (String)params["element"];

    Array targets = ctx->get_targets();
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        Node *node = Object::cast_to<Node>(v);
        if (node) {
            StatusEffect *eff = memnew(StatusEffect);
            eff->configure(amount, interval, duration, element);
            node->add_child(eff);
        }
    }
}

void DotExecutor::_bind_methods() {
}
