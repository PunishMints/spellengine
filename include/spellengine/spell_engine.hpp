// SpellEngine: core factory / composition / execution orchestration
#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include "spellengine/aspect.hpp"
#include "spellengine/spell.hpp"

class SpellCaster;

using namespace godot;

class SpellEngine : public Object {
    GDCLASS(SpellEngine, Object)

protected:
    static void _bind_methods();

private:
    // map param key -> default merge mode
    Dictionary default_merge_modes;
    // map merge mode (int) -> mana multiplier
    Dictionary merge_mode_multipliers;
    // verbose logging of composition stages
    bool verbose_composition = false;
    // incrementing counter for generated cast ids
    uint64_t cast_counter = 0;

public:
    // Build a Spell from a list of aspects. Currently this simply aggregates components.
    Ref<Spell> build_spell_from_aspects(const TypedArray<Ref<Aspect>> &aspects);

    // Execute a spell with a given context
    void execute_spell(Ref<Spell> spell, Ref<SpellContext> ctx);

        // Merge mode constants
        enum MergeMode {
            MERGE_OVERWRITE = 0,
            MERGE_ADD = 1,
            MERGE_MULTIPLY = 2,
            MERGE_MIN = 3,
            MERGE_MAX = 4
        };

        // Set default merge mode for a given parameter key (used if not provided in context)
        void set_default_merge_mode(const String &key, int mode);
        int get_default_merge_mode(const String &key) const;

        // Set multiplicative mana cost modifier associated with a merge mode
        void set_merge_mode_mana_multiplier(int mode, double multiplier);
        double get_merge_mode_mana_multiplier(int mode) const;

    // Resolve a single component's parameters given the casting aspects (simple weighted rules).
    // Optionally provide the SpellCaster to apply per-aspect caster scalers when computing final numeric values.
    Dictionary resolve_component_params(Ref<SpellComponent> component, const Array &casting_aspects, SpellCaster *caster = nullptr, const Dictionary &ctx_params = Dictionary());

    // Helper: compute adjusted mana costs for a whole spell given a context
    // Returns a Dictionary: {"costs_per_aspect": Dictionary, "total_mana": float, "per_component": Dictionary}
    Dictionary get_adjusted_mana_costs(Ref<Spell> spell, Ref<SpellContext> ctx);

    // Verbose composition logging control
    void set_verbose_composition(bool v);
    bool get_verbose_composition() const;
};
