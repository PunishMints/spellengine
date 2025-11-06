// Knockback executor: applies radial impulse to targets
#pragma once

#include "executor_base.hpp"

using namespace godot;

class KnockbackExecutor : public IExecutor {
    GDCLASS(KnockbackExecutor, IExecutor)

protected:
    static void _bind_methods();

public:
    virtual void execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) override;
    virtual String get_executor_id() const override;
    virtual Dictionary get_param_schema() const override;
};
