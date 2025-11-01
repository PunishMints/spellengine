// SpellComponent resource: data-only description of a spell fragment
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

class SpellComponent : public Resource {
    GDCLASS(SpellComponent, Resource)

protected:
    static void _bind_methods();

private:
    String executor_id = ""; // identifies which executor implements this component
    int priority = 0;
    double cost = 0.0;
    // base parameters for the component (e.g., damage, radius, duration)
    Dictionary base_params;

    // aspect contributions: aspect_id -> share (float). Sum will be normalized at runtime.
    Dictionary aspects_contributions;

    // per-aspect modifiers: aspect_id -> Dictionary of param modifiers
    Dictionary aspect_modifiers;

    // per-component synergy modifiers: synergy_key -> Dictionary
    Dictionary synergy_modifiers;

public:
    String get_executor_id() const;
    void set_executor_id(const String &p_id);

    int get_priority() const;
    void set_priority(int p);

    double get_cost() const;
    void set_cost(double c);

    Dictionary get_params() const; // legacy alias to base_params
    void set_params(const Dictionary &p_params);

    Dictionary get_base_params() const;
    void set_base_params(const Dictionary &p);

    Dictionary get_aspects_contributions() const;
    void set_aspects_contributions(const Dictionary &d);

    Dictionary get_aspect_modifiers() const;
    void set_aspect_modifiers(const Dictionary &d);

    Dictionary get_synergy_modifiers() const;
    void set_synergy_modifiers(const Dictionary &d);
};
