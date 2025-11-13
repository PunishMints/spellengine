#include "spellengine/spell_engine.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "spellengine/executor_registry.hpp"
#include "spellengine/executor_base.hpp"
#include "spellengine/spell_caster.hpp"
#include "spellengine/synergy_registry.hpp"
#include <godot_cpp/variant/callable.hpp>
#include "spellengine/control_orchestrator.hpp"
#include "spellengine/aspect.hpp"
#include <algorithm>
#include <cmath>

using namespace godot;

// Helper: merge two scalers according to mode
static double merge_scalers(double existing, double incoming, int mode) {
    switch (mode) {
        case SpellEngine::MERGE_ADD: return existing + incoming;
        case SpellEngine::MERGE_MULTIPLY: return existing * incoming;
        case SpellEngine::MERGE_MIN: return std::min(existing, incoming);
        case SpellEngine::MERGE_MAX: return std::max(existing, incoming);
        case SpellEngine::MERGE_OVERWRITE:
        default:
            return incoming;
    }
}

Ref<Spell> SpellEngine::build_spell_from_aspects(const TypedArray<Ref<Aspect>> &aspects) {
    Ref<Spell> spell = memnew(Spell);

    TypedArray<Ref<SpellComponent>> agg_components;
    for (int i = 0; i < aspects.size(); ++i) {
        Ref<Aspect> a = aspects[i];
        if (!a.is_valid()) continue;
        TypedArray<Ref<SpellComponent>> comps = a->get_components();
        for (int j = 0; j < comps.size(); ++j) {
            agg_components.push_back(comps[j]);
        }
    }

    spell->set_components(agg_components);
    spell->set_source_template("built_from_aspects");
    return spell;
}

void SpellEngine::set_default_merge_mode(const String &key, int mode) {
    default_merge_modes[key] = mode;
}

int SpellEngine::get_default_merge_mode(const String &key) const {
    if (default_merge_modes.has(key)) return (int)default_merge_modes[key];
    return MERGE_OVERWRITE;
}

void SpellEngine::set_merge_mode_mana_multiplier(int mode, double multiplier) {
    merge_mode_multipliers[mode] = multiplier;
}

double SpellEngine::get_merge_mode_mana_multiplier(int mode) const {
    if (merge_mode_multipliers.has(mode)) return (double)merge_mode_multipliers[mode];
    return 1.0;
}

void SpellEngine::execute_spell(Ref<Spell> spell, Ref<SpellContext> ctx) {
    if (!spell.is_valid()) {
        UtilityFunctions::print("SpellEngine: invalid spell passed to execute_spell");
        return;
    }

    Array casting_aspects;
    Dictionary ctx_params = ctx->get_params();

    // generate a unique cast id for this spell execution (propagated to executors and synergies)
    cast_counter += 1;
    String cast_id = "spell_cast_" + String::num(cast_counter);
    if (ctx_params.has("aspects")) {
        Variant v = ctx_params["aspects"];
        if (v.get_type() == Variant::ARRAY) casting_aspects = v;
    }

    Node *caster_node = ctx->get_caster();
    if (casting_aspects.size() == 0 && caster_node) {
        SpellCaster *sc = Object::cast_to<SpellCaster>(caster_node);
        if (sc) casting_aspects = sc->get_assigned_aspects();
    }

    TypedArray<Ref<SpellComponent>> comps = spell->get_components();
    for (int i = 0; i < comps.size(); ++i) {
        Ref<SpellComponent> comp = comps[i];
        if (!comp.is_valid()) continue;

        SpellCaster *sc_for_resolve = nullptr;
        if (caster_node) sc_for_resolve = Object::cast_to<SpellCaster>(caster_node);

    Dictionary resolved = resolve_component_params(comp, casting_aspects, sc_for_resolve, ctx_params);
        if (verbose_composition) {
            UtilityFunctions::print(String("[SpellEngine] Resolved component '") + comp->get_executor_id() + "':");
            if (resolved.has("resolved_params")) UtilityFunctions::print(String("  resolved_params: ") + String::num(resolved["resolved_params"].get_type()));
            if (resolved.has("cost_per_aspect")) UtilityFunctions::print(String("  cost_per_aspect: ") + String::num(resolved["cost_per_aspect"].get_type()));
        }
        Dictionary resolved_params;
        if (resolved.has("resolved_params")) resolved_params = resolved["resolved_params"];

        Dictionary cost_per_aspect;
        if (resolved.has("cost_per_aspect")) cost_per_aspect = resolved["cost_per_aspect"];

        bool can_cast = true;
        SpellCaster *sc = nullptr;
        if (caster_node) sc = Object::cast_to<SpellCaster>(caster_node);

        Array cost_keys = cost_per_aspect.keys();
        for (int ci = 0; ci < cost_keys.size(); ++ci) {
            String aspect = cost_keys[ci];
            double need = (double)cost_per_aspect[aspect];
            if (!sc) { can_cast = false; break; }
            if (!sc->can_deduct(aspect, need)) { can_cast = false; break; }
        }

        if (!can_cast) {
            UtilityFunctions::print(String("SpellEngine: caster lacks mana for component: ") + comp->get_executor_id());
            return;
        }

        for (int ci = 0; ci < cost_keys.size(); ++ci) {
            String aspect = cost_keys[ci];
            double need = (double)cost_per_aspect[aspect];
            sc->deduct_mana(aspect, need);
        }

        ExecutorRegistry *reg = ExecutorRegistry::get_singleton();
        // Skip control-only components (those that begin with choose_ or select_)
        // They are handled earlier by ControlOrchestrator via resolve_controls and
        // should not be executed as normal executors during execute_spell.
        String comp_exec_id = comp->get_executor_id();
        if (comp_exec_id.begins_with("choose_") || comp_exec_id.begins_with("select_")) {
            if (verbose_composition) UtilityFunctions::print(String("SpellEngine: skipping control-only component execution for: ") + comp_exec_id);
        } else if (reg && reg->has_executor(comp->get_executor_id())) {
            Ref<IExecutor> exec = reg->get_executor(comp->get_executor_id());
            if (exec.is_valid()) {
                // ensure the resolved params carry the cast id so targets can group events
                Dictionary rp_with_cast = resolved_params;
                rp_with_cast["cast_id"] = cast_id;
                exec->execute(ctx, comp, rp_with_cast);
            }
        } else {
            UtilityFunctions::print(String("SpellEngine: no executor registered for: ") + comp->get_executor_id());
        }

        if (resolved.has("aspects_used")) {
            Array aspects_used = resolved["aspects_used"];
            Array sorted = aspects_used.duplicate();
            sorted.sort();
            String skey = "";
            for (int si = 0; si < sorted.size(); ++si) {
                if (si) skey += "+";
                skey += (String)sorted[si];
            }
            // normalize for registry lookup
            String skey_lower = skey.to_lower();
            UtilityFunctions::print(String("[SpellEngine] execute_spell combined key: '") + skey + "' -> '" + skey_lower + "'");

            SynergyRegistry *sreg = SynergyRegistry::get_singleton();
            if (sreg && sreg->has_synergy(skey_lower)) {
                Dictionary spec = sreg->get_synergy(skey_lower);
                if (spec.has("callable")) {
                    Variant cv = spec["callable"];
                    if (cv.get_type() == Variant::CALLABLE) {
                        Callable cb = cv;
                        Array args;
                        args.push_back(ctx);
                        args.push_back(comp);
                        // pass a copy of resolved_params augmented with cast_id
                        Dictionary rp_with_cast = resolved_params;
                        rp_with_cast["cast_id"] = cast_id;
                        args.push_back(rp_with_cast);
                        args.push_back(spec);
                        // pass the canonical (lowercased) synergy key
                        args.push_back(skey_lower);
                        cb.callv(args);
                    } else if (cv.get_type() == Variant::ARRAY) {
                        Array carray = cv;
                        for (int ci = 0; ci < carray.size(); ++ci) {
                            Variant item = carray[ci];
                            if (item.get_type() == Variant::CALLABLE) {
                                Callable cb = item;
                                Array args;
                                args.push_back(ctx);
                                args.push_back(comp);
                                Dictionary rp_with_cast = resolved_params;
                                rp_with_cast["cast_id"] = cast_id;
                                args.push_back(rp_with_cast);
                                args.push_back(spec);
                                // pass the canonical (lowercased) synergy key
                                args.push_back(skey_lower);
                                cb.callv(args);
                            }
                        }
                    }
                }
                if (spec.has("extra_executors")) {
                    Variant ev = spec["extra_executors"];
                    if (ev.get_type() == Variant::ARRAY) {
                        Array extras = ev;
                        UtilityFunctions::print(String("[SpellEngine] synergy '") + skey_lower + "' has extra_executors count=" + String::num(extras.size()));
                        for (int ei = 0; ei < extras.size(); ++ei) {
                            Variant exv = extras[ei];
                            UtilityFunctions::print(String("[SpellEngine] examining extra_executors[") + String::num(ei) + "]:");
                            UtilityFunctions::print(exv);
                            if (exv.get_type() != Variant::DICTIONARY) continue;
                            Dictionary exd = exv;

                            if (exd.has("trigger_on_executor")) {
                                Variant tov = exd["trigger_on_executor"];
                                bool trigger_ok = false;
                                if (tov.get_type() == Variant::STRING) {
                                    String trig = tov;
                                    if (trig.to_lower() == comp->get_executor_id().to_lower()) trigger_ok = true;
                                } else if (tov.get_type() == Variant::ARRAY) {
                                    Array tarr = tov;
                                    for (int ti = 0; ti < tarr.size(); ++ti) {
                                        Variant tv = tarr[ti];
                                        if (tv.get_type() != Variant::STRING) continue;
                                        if (((String)tv).to_lower() == comp->get_executor_id().to_lower()) {
                                            trigger_ok = true; break;
                                        }
                                    }
                                }
                                if (!trigger_ok) {
                                    UtilityFunctions::print(String("[SpellEngine] extra_executors entry skipped due to trigger mismatch; expected:"));
                                    UtilityFunctions::print(exd["trigger_on_executor"]);
                                    UtilityFunctions::print(String("[SpellEngine] got: ") + comp->get_executor_id());
                                    continue;
                                }
                            }

                            if (!exd.has("executor_id")) continue;
                            String extra_exec_id = exd["executor_id"];

                            Dictionary extra_params;
                            bool use_resolved = true;
                            if (exd.has("use_resolved_params")) {
                                Variant ur = exd["use_resolved_params"];
                                if (ur.get_type() == Variant::BOOL) use_resolved = (bool)ur;
                            }
                            if (use_resolved) extra_params = resolved_params;
                            if (exd.has("params_mods")) {
                                Variant pmv = exd["params_mods"];
                                if (pmv.get_type() == Variant::DICTIONARY) {
                                    Dictionary pmd = pmv;
                                    Array pk = pmd.keys();
                                    for (int pki = 0; pki < pk.size(); ++pki) {
                                        String pkey = pk[pki];
                                        extra_params[pkey] = pmd[pkey];
                                    }
                                }
                            }

                            bool charge_cost = false;
                            double extra_cost = 0.0;
                            if (exd.has("charge_cost")) {
                                Variant cv = exd["charge_cost"];
                                if (cv.get_type() == Variant::BOOL) charge_cost = (bool)cv;
                            }
                            if (exd.has("cost")) {
                                Variant ccv = exd["cost"];
                                if (ccv.get_type() == Variant::INT || ccv.get_type() == Variant::FLOAT) extra_cost = (double)ccv;
                            }

                            if (charge_cost && extra_cost > 0.0) {
                                Dictionary extra_cost_per_aspect;
                                double sum_shares = 0.0;
                                Dictionary main_costs;
                                if (resolved.has("cost_per_aspect")) {
                                    Variant mv = resolved["cost_per_aspect"];
                                    if (mv.get_type() == Variant::DICTIONARY) main_costs = mv;
                                }

                                if (main_costs.size() > 0) {
                                    Array mkeys = main_costs.keys();
                                    for (int mk = 0; mk < mkeys.size(); ++mk) {
                                        String a = mkeys[mk];
                                        if (main_costs.has(a)) {
                                            Variant vv = main_costs[a];
                                            if (vv.get_type() == Variant::INT || vv.get_type() == Variant::FLOAT) sum_shares += (double)vv;
                                        }
                                    }
                                }

                                for (int ai = 0; ai < aspects_used.size(); ++ai) {
                                    String a = aspects_used[ai];
                                    double share = 0.0;
                                    if (sum_shares > 0.0 && main_costs.has(a)) {
                                        Variant vv = main_costs[a];
                                        if (vv.get_type() == Variant::INT || vv.get_type() == Variant::FLOAT) share = (double)vv / sum_shares;
                                    } else {
                                        share = 1.0 / (double)aspects_used.size();
                                    }
                                    extra_cost_per_aspect[a] = extra_cost * share;
                                }

                                Array ekeys = extra_cost_per_aspect.keys();
                                bool ok = true;
                                if (!sc) ok = false;
                                for (int k = 0; k < ekeys.size() && ok; ++k) {
                                    String a = ekeys[k];
                                    double need = (double)extra_cost_per_aspect[a];
                                    if (!sc->can_deduct(a, need)) ok = false;
                                }
                                if (!ok) continue;
                                for (int k = 0; k < ekeys.size(); ++k) {
                                    String a = ekeys[k];
                                    double need = (double)extra_cost_per_aspect[a];
                                    sc->deduct_mana(a, need);
                                }
                            }

                            if (reg && reg->has_executor(extra_exec_id)) {
                                Ref<IExecutor> extra_exec = reg->get_executor(extra_exec_id);
                                if (extra_exec.is_valid()) {
                                    // ensure extra executor params include the cast id
                                    Dictionary extra_with_cast = extra_params;
                                    extra_with_cast["cast_id"] = cast_id;
                                    extra_exec->execute(ctx, comp, extra_with_cast);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

Dictionary SpellEngine::get_adjusted_mana_costs(Ref<Spell> spell, Ref<SpellContext> ctx) {
    Dictionary out;
    Dictionary total_per_aspect;
    double total_mana = 0.0;
    if (!spell.is_valid() || !ctx.is_valid()) {
        out["costs_per_aspect"] = total_per_aspect;
        out["total_mana"] = total_mana;
        out["per_component"] = Dictionary();
        return out;
    }

    Array comps_arr = spell->get_components();
    Dictionary per_component;
    Dictionary ctx_params = ctx->get_params();

    Node *caster_node = ctx->get_caster();
    SpellCaster *sc_for_resolve = nullptr;
    if (caster_node) sc_for_resolve = Object::cast_to<SpellCaster>(caster_node);

    // Determine casting aspects similar to execute_spell
    Array casting_aspects;
    if (ctx_params.has("aspects")) {
        Variant v = ctx_params["aspects"];
        if (v.get_type() == Variant::ARRAY) casting_aspects = v;
    }

    if (casting_aspects.size() == 0 && caster_node) {
        if (sc_for_resolve) casting_aspects = sc_for_resolve->get_assigned_aspects();
    }

    for (int i = 0; i < comps_arr.size(); ++i) {
        Ref<SpellComponent> comp = comps_arr[i];
        if (!comp.is_valid()) continue;
        Dictionary resolved = resolve_component_params(comp, casting_aspects, sc_for_resolve, ctx_params);
        Dictionary comp_costs;
        if (resolved.has("cost_per_aspect")) comp_costs = resolved["cost_per_aspect"];
        Array k = comp_costs.keys();
        for (int ki = 0; ki < k.size(); ++ki) {
            String a = k[ki];
            double v = 0.0;
            Variant vv = comp_costs[a];
            if (vv.get_type() == Variant::INT || vv.get_type() == Variant::FLOAT) v = (double)vv;
            if (total_per_aspect.has(a)) total_per_aspect[a] = (double)total_per_aspect[a] + v;
            else total_per_aspect[a] = v;
            total_mana += v;
        }
        per_component[comp->get_executor_id()] = comp_costs;
    }

    out["costs_per_aspect"] = total_per_aspect;
    out["total_mana"] = total_mana;
    out["per_component"] = per_component;
    return out;
}

void SpellEngine::set_verbose_composition(bool v) {
    verbose_composition = v;
}

bool SpellEngine::get_verbose_composition() const {
    return verbose_composition;
}

Dictionary SpellEngine::resolve_component_params(Ref<SpellComponent> component, const Array &casting_aspects, SpellCaster *caster, const Dictionary &ctx_params) {
    Dictionary out;
    if (!component.is_valid()) return out;

    Dictionary base = component->get_base_params();

    Dictionary contribs = component->get_aspects_contributions();
    Array aspects_used;
    Dictionary normalized;

    if (contribs.size() == 0) {
        if (casting_aspects.size() > 0) {
            String a = casting_aspects[0];
            normalized[a] = 1.0;
            aspects_used.push_back(a);
        }
    } else {
        double sum = 0.0;
        for (int i = 0; i < casting_aspects.size(); ++i) {
            String a = casting_aspects[i];
            if (contribs.has(a)) {
                double v = (double)contribs[a];
                normalized[a] = v;
                sum += v;
                aspects_used.push_back(a);
            }
        }
        if (aspects_used.size() == 0) {
            Array keys = contribs.keys();
            for (int i = 0; i < keys.size(); ++i) {
                String a = keys[i];
                double v = (double)contribs[a];
                normalized[a] = v;
                sum += v;
                aspects_used.push_back(a);
            }
        }
        if (sum <= 0.0) {
            double even = 1.0 / (double)aspects_used.size();
            for (int i = 0; i < aspects_used.size(); ++i) normalized[aspects_used[i]] = even;
        } else {
            for (int i = 0; i < aspects_used.size(); ++i) {
                String a = aspects_used[i];
                normalized[a] = (double)normalized[a] / sum;
            }
        }
    }

    Dictionary aspect_mods = component->get_aspect_modifiers();
    Dictionary resolved_params;

    Array base_keys = base.keys();
    for (int k = 0; k < base_keys.size(); ++k) {
        Variant key = base_keys[k];
        Variant base_val = base[key];
        if (base_val.get_type() == Variant::INT || base_val.get_type() == Variant::FLOAT) {
            double base_num = (double)base_val;
            double acc = 0.0;
            if (verbose_composition) {
                UtilityFunctions::print(String("[SpellEngine] composing param '") + (String)key + "' base=" + String::num((double)base_val));
            }
            for (int i = 0; i < aspects_used.size(); ++i) {
                String a = aspects_used[i];
                double share = (double)normalized[a];
                double mod = 1.0;
                if (aspect_mods.has(a)) {
                    Variant vm = aspect_mods[a];
                    if (vm.get_type() == Variant::DICTIONARY) {
                        Dictionary md = vm;
                        if (md.has((String)key)) {
                            Variant mm = md[(String)key];
                            if (mm.get_type() == Variant::INT || mm.get_type() == Variant::FLOAT) mod = (double)mm;
                        }
                    }
                }
                if (verbose_composition) {
                    UtilityFunctions::print(String("    aspect '") + a + "' share=" + String::num(share) + " mod=" + String::num(mod));
                }
                acc += share * (base_num * mod);
            }
            double final_val = acc;
            // during composition, merge caster scaler and aspect default scalers per-key according to merge modes
            double mult = 0.0;
            for (int i = 0; i < aspects_used.size(); ++i) {
                String a = aspects_used[i];
                double share = (double)normalized[a];

                // aspect default scaler: prefer values on the Aspect resource, but fall back to a single-aspect Synergy resource's default_scalers
                double aspect_default = 1.0;
                bool found_aspect_default = false;
                // Prefer default scalers from the Synergy resource for the single aspect.
                // Aspect resources are treated as presentation-only (text/visuals).
                SynergyRegistry *sreg_single = SynergyRegistry::get_singleton();
                if (sreg_single) {
                    String lookup_key = a;
                    if (!sreg_single->has_synergy(lookup_key)) lookup_key = a.to_lower();
                    if (sreg_single->has_synergy(lookup_key)) {
                        Dictionary spec_single = sreg_single->get_synergy(lookup_key);
                        if (spec_single.has("default_scalers")) {
                            Variant sv = spec_single["default_scalers"];
                            if (sv.get_type() == Variant::DICTIONARY) {
                                Dictionary sdefs = sv;
                                if (sdefs.has((String)key)) {
                                    Variant dv = sdefs[(String)key];
                                    if (dv.get_type() == Variant::INT || dv.get_type() == Variant::FLOAT) {
                                        aspect_default = (double)dv;
                                        found_aspect_default = true;
                                    }
                                }
                            }
                        }
                    }
                }

                double caster_scaler = 1.0;
                if (caster) caster_scaler = caster->get_scaler(a, (String)key);

                // determine merge mode: prefer context override, else engine default, else overwrite
                int mode = MERGE_OVERWRITE;
                if (ctx_params.has("merge_modes")) {
                    Variant mmv = ctx_params["merge_modes"];
                    if (mmv.get_type() == Variant::DICTIONARY) {
                        Dictionary md = mmv;
                        if (md.has((String)key)) {
                            Variant mv = md[(String)key];
                            if (mv.get_type() == Variant::INT) mode = (int)mv;
                        }
                    }
                }
                if (mode == MERGE_OVERWRITE) {
                    if (default_merge_modes.has((String)key)) {
                        Variant dv = default_merge_modes[(String)key];
                        if (dv.get_type() == Variant::INT) mode = (int)dv;
                    }
                }
                Variant dm = Dictionary();
                // Merge existing caster and aspect_default according to mode
                double merged = merge_scalers(caster_scaler, aspect_default, mode);
                if (verbose_composition) {
                    UtilityFunctions::print(String("    merging scaler for aspect '") + a + "' key='" + (String)key + "' caster_scaler=" + String::num(caster_scaler) + " aspect_default=" + String::num(aspect_default) + " mode=" + String::num(mode) + " => merged=" + String::num(merged));
                }
                mult += share * merged;
            }
            if (mult <= 0.0) mult = 1.0;
            final_val = acc * mult;
            resolved_params[key] = final_val;
        } else {
            resolved_params[key] = base_val;
        }
    }

    double total_cost = component->get_cost();
    // If the component defines a mana_cost in its base params, use that as the starting point
    if (resolved_params.has("mana_cost")) {
        Variant mc = resolved_params["mana_cost"];
        if (mc.get_type() == Variant::INT || mc.get_type() == Variant::FLOAT) total_cost = (double)mc;
    }

    // Apply per-aspect and caster scalers to mana_cost even if mana_cost wasn't present in base.
    // Compute a merged multiplier across aspects (weighted by normalized shares).
    double mana_scaler_mult = 0.0;
    for (int i = 0; i < aspects_used.size(); ++i) {
        String a = aspects_used[i];
        double share = (double)normalized[a];

        double aspect_default = 1.0;
        bool found_aspect_default = false;
        // Prefer mana_cost default scalers from the single-aspect Synergy resource.
        SynergyRegistry *sreg_single_mc = SynergyRegistry::get_singleton();
        if (sreg_single_mc) {
            String lookup_key = a;
            if (!sreg_single_mc->has_synergy(lookup_key)) lookup_key = a.to_lower();
            if (sreg_single_mc->has_synergy(lookup_key)) {
                Dictionary spec_single = sreg_single_mc->get_synergy(lookup_key);
                if (spec_single.has("default_scalers")) {
                    Variant sv = spec_single["default_scalers"];
                    if (sv.get_type() == Variant::DICTIONARY) {
                        Dictionary sdefs = sv;
                        if (sdefs.has("mana_cost")) {
                            Variant dv = sdefs["mana_cost"];
                            if (dv.get_type() == Variant::INT || dv.get_type() == Variant::FLOAT) aspect_default = (double)dv;
                        }
                    }
                }
            }
        }

        double caster_scaler = 1.0;
        if (caster) caster_scaler = caster->get_scaler(a, "mana_cost");

        // resolve merge mode for mana_cost (context override -> engine default -> overwrite)
        int mode = MERGE_OVERWRITE;
        if (ctx_params.has("merge_modes")) {
            Variant mmv = ctx_params["merge_modes"];
            if (mmv.get_type() == Variant::DICTIONARY) {
                Dictionary md = mmv;
                if (md.has("mana_cost")) {
                    Variant mv = md["mana_cost"];
                    if (mv.get_type() == Variant::INT) mode = (int)mv;
                }
            }
        }
        if (mode == MERGE_OVERWRITE) {
            if (default_merge_modes.has("mana_cost")) {
                Variant dv = default_merge_modes["mana_cost"];
                if (dv.get_type() == Variant::INT) mode = (int)dv;
            }
        }

        double merged = merge_scalers(caster_scaler, aspect_default, mode);
        if (verbose_composition) {
            UtilityFunctions::print(String("[SpellEngine] mana cost scaler for aspect '") + a + "' caster=" + String::num(caster_scaler) + " aspect_default=" + String::num(aspect_default) + " mode=" + String::num(mode) + " => merged=" + String::num(merged) + " share=" + String::num(share));
        }
        mana_scaler_mult += share * merged;
    }
    if (mana_scaler_mult <= 0.0) mana_scaler_mult = 1.0;
    total_cost *= mana_scaler_mult;
    // update resolved_params so callers see final mana_cost
    resolved_params["mana_cost"] = total_cost;
    // Apply merge-mode mana multiplier if defined for the 'mana_cost' key
    double mana_multiplier = 1.0;
    if (resolved_params.has("mana_cost")) {
        // determine merge mode for mana_cost (context override -> engine default -> overwrite)
        int mana_mode = MERGE_MULTIPLY;
        if (ctx_params.has("merge_modes")) {
            Variant mmv = ctx_params["merge_modes"];
            if (mmv.get_type() == Variant::DICTIONARY) {
                Dictionary md = mmv;
                if (md.has("mana_cost")) {
                    Variant mv = md["mana_cost"];
                    if (mv.get_type() == Variant::INT) mana_mode = (int)mv;
                }
            }
        }
        if (mana_mode == MERGE_OVERWRITE) {
            if (default_merge_modes.has("mana_cost")) {
                Variant dv = default_merge_modes["mana_cost"];
                if (dv.get_type() == Variant::INT) mana_mode = (int)dv;
            }
        }

        // fetch multiplier for the resolved mana_mode
        mana_multiplier = get_merge_mode_mana_multiplier(mana_mode);
        total_cost *= mana_multiplier;
    }
    if (verbose_composition) {
        UtilityFunctions::print(String("[SpellEngine] final total_cost for component '") + component->get_executor_id() + "' = " + String::num(total_cost) + " (mana_multiplier=" + String::num(mana_multiplier) + ")");
    }
    Dictionary cost_per_aspect;
    for (int i = 0; i < aspects_used.size(); ++i) {
        String a = aspects_used[i];
        double share = (double)normalized[a];
        cost_per_aspect[a] = total_cost * share;
    }

    Dictionary synergy_mods = component->get_synergy_modifiers();
    if (synergy_mods.size() > 0 || SynergyRegistry::get_singleton()->get_synergy_keys().size() > 0) {
        Array sorted = aspects_used.duplicate();
        sorted.sort();
        String key = "";
        for (int i = 0; i < sorted.size(); ++i) {
            if (i) key += "+";
            key += (String)sorted[i];
        }
    // normalize combined key to lowercase for registry lookups (synergies are registered with
    // lowercase keys). When checking component-level synergy_modifiers, accept either the
    // original-cased key or the lowercased form for backwards compatibility.
    String key_lower = key.to_lower();
    UtilityFunctions::print(String("[SpellEngine] combined synergy key built: '") + key + "' -> '" + key_lower + "'");

        if (synergy_mods.size() > 0 && (synergy_mods.has(key) || synergy_mods.has(key_lower))) {
            Variant sv = synergy_mods[key];
            if (!sv.get_type() || sv.get_type() == Variant::NIL) sv = synergy_mods[key_lower];
            if (sv.get_type() == Variant::DICTIONARY) {
                Dictionary sm = sv;
                Array sk = sm.keys();
                for (int i = 0; i < sk.size(); ++i) {
                    String skey = sk[i];
                    resolved_params[skey] = sm[skey];
                }
            }
        }

        SynergyRegistry *sreg = SynergyRegistry::get_singleton();
        if (sreg) {
            if (sreg->has_synergy(key_lower)) {
                UtilityFunctions::print(String("[SpellEngine] found synergy in registry for key: ") + key_lower);
                Dictionary spec = sreg->get_synergy(key_lower);
                // Apply synergy-level default_scalers (scale existing numeric resolved params)
                // Only apply combined-key synergy scalers when multiple aspects are involved.
                // Single-aspect defaults are already sourced per-aspect above from the
                // single-aspect synergy resource.
                if (aspects_used.size() > 1 && spec.has("default_scalers")) {
                    Variant sv = spec["default_scalers"];
                    if (sv.get_type() == Variant::DICTIONARY) {
                        Dictionary sdefs = sv;
                        Array sk = sdefs.keys();
                        for (int si = 0; si < sk.size(); ++si) {
                            String skey = sk[si];
                            Variant sval = sdefs[skey];
                            if (!(sval.get_type() == Variant::INT || sval.get_type() == Variant::FLOAT)) continue;
                            double scaler = (double)sval;
                            // if resolved param exists and is numeric, multiply it
                            if (resolved_params.has(skey)) {
                                Variant rv = resolved_params[skey];
                                if (rv.get_type() == Variant::INT || rv.get_type() == Variant::FLOAT) {
                                    resolved_params[skey] = (double)rv * scaler;
                                }
                            } else {
                                // If param not present, set it to scaler (applies to params like mana_cost, fallback handled elsewhere)
                                resolved_params[skey] = scaler;
                            }
                        }
                    }
                }

                if (spec.has("executor_overrides")) {
                    Variant ev = spec["executor_overrides"];
                    if (ev.get_type() == Variant::DICTIONARY) {
                        Dictionary ed = ev;
                        String exec_id = component->get_executor_id();
                        if (ed.has(exec_id)) {
                            Variant hev = ed[exec_id];
                            if (hev.get_type() == Variant::DICTIONARY) {
                                Dictionary hev_d = hev;
                                if (hev_d.has("params_mods")) {
                                    Variant pmv = hev_d["params_mods"];
                                    if (pmv.get_type() == Variant::DICTIONARY) {
                                        Dictionary pmd = pmv;
                                        Array pk = pmd.keys();
                                        for (int i = 0; i < pk.size(); ++i) {
                                            String pkey = pk[i];
                                            resolved_params[pkey] = pmd[pkey];
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else {
                UtilityFunctions::print(String("[SpellEngine] no combined-key synergy found for: ") + key_lower);
            }
        }
    }

    out["resolved_params"] = resolved_params;
    out["cost_per_aspect"] = cost_per_aspect;
    out["aspects_used"] = aspects_used;
    return out;
}

void SpellEngine::_bind_methods() {
    ClassDB::bind_method(D_METHOD("build_spell_from_aspects", "aspects"), &SpellEngine::build_spell_from_aspects);
    ClassDB::bind_method(D_METHOD("execute_spell", "spell", "context"), &SpellEngine::execute_spell);
    ClassDB::bind_method(D_METHOD("set_default_merge_mode", "key", "mode"), &SpellEngine::set_default_merge_mode);
    ClassDB::bind_method(D_METHOD("get_default_merge_mode", "key"), &SpellEngine::get_default_merge_mode);
    ClassDB::bind_method(D_METHOD("set_merge_mode_mana_multiplier", "mode", "multiplier"), &SpellEngine::set_merge_mode_mana_multiplier);
    ClassDB::bind_method(D_METHOD("get_merge_mode_mana_multiplier", "mode"), &SpellEngine::get_merge_mode_mana_multiplier);
    ClassDB::bind_method(D_METHOD("get_adjusted_mana_costs", "spell", "context"), &SpellEngine::get_adjusted_mana_costs);
    ClassDB::bind_method(D_METHOD("collect_controls", "spell", "context"), &SpellEngine::collect_controls);
    ClassDB::bind_method(D_METHOD("validate_control_result", "mode", "result"), &SpellEngine::validate_control_result);
    ClassDB::bind_method(D_METHOD("resolve_controls", "spell", "context", "parent", "on_complete"), &SpellEngine::resolve_controls);
    ClassDB::bind_method(D_METHOD("_resolve_controls_forward", "out", "orch", "orig_cb"), &SpellEngine::_resolve_controls_forward);
    ClassDB::bind_method(D_METHOD("set_verbose_composition", "enabled"), &SpellEngine::set_verbose_composition);
    ClassDB::bind_method(D_METHOD("get_verbose_composition"), &SpellEngine::get_verbose_composition);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "verbose_composition"), "set_verbose_composition", "get_verbose_composition");

}

void SpellEngine::resolve_controls(Ref<Spell> spell, Ref<SpellContext> ctx, Node *parent, const Callable &on_complete) {
    if (!spell.is_valid() || !ctx.is_valid()) {
        Array args;
        Dictionary out;
        out["ok"] = false;
        out["error"] = String("invalid_args");
        args.append(out);
        if (on_complete.is_valid()) on_complete.callv(args);
        return;
    }

    // Determine control components and their indices so we can run components
    // preceding the first control, then resolve inputs, then run the
    // remainder of the spell.
    Array controls = collect_controls(spell, ctx);
    int first_control_index = INT32_MAX;
    for (int i = 0; i < controls.size(); ++i) {
        Variant v = controls[i];
        if (v.get_type() != Variant::DICTIONARY) continue;
        Dictionary d = v;
        Variant idxv = d.get(Variant("index"), Variant());
        if (idxv.get_type() == Variant::INT) {
            int idx = (int)idxv;
            if (idx < first_control_index) first_control_index = idx;
        }
    }
    if (first_control_index == INT32_MAX) first_control_index = 0;

    // Execute prefix components up to the first control (exclusive). This
    // preserves ordering: components defined before the first interactive
    // control run immediately.
    if (first_control_index > 0) {
        execute_components_range(spell, ctx, 0, first_control_index);
    }

    // If a prefix executor signalled a failure via ctx.results.executor_failed,
    // abort resolution and forward the failure to the caller.
    if (ctx.is_valid()) {
        Dictionary r = ctx->get_results();
        if (r.has("executor_failed")) {
            Dictionary err = r["executor_failed"];
            Array args;
            Dictionary out;
            out["ok"] = false;
            out["error"] = String("executor_failed_before_controls");
            out["detail"] = err;
            args.append(out);
            if (on_complete.is_valid()) on_complete.callv(args);
            return;
        }
    }

    // If there are no controls at all, finish by calling on_complete (spell already executed)
    if (controls.size() == 0) {
        Array args;
        Dictionary out;
        out["ok"] = true;
        out["context"] = ctx;
        args.append(out);
        if (on_complete.is_valid()) on_complete.callv(args);
        return;
    }

    // Then: create an orchestrator to resolve the interactive controls for
    // the remaining components and execute them when done.
    ControlOrchestrator *orch = memnew(ControlOrchestrator());
    // record the first control index on the orchestrator so it can run the
    // remainder after controls are resolved
    orch->set_first_control_index(first_control_index);

    // Create a wrapper callable bound to this SpellEngine instance that will
    // receive the orchestrator's out dictionary, then forward the result to the
    // original on_complete callable. We bind the raw pointer here; the
    // orchestrator will schedule its own deferred cleanup after the
    // completion callback returns to avoid use-after-free.
    Callable wrapper = Callable(this, "_resolve_controls_forward").bind(Variant(orch), Variant(on_complete));

    // Start orchestration (async). The orchestrator will call the wrapper with a single
    // Dictionary argument (the result), which will be forwarded by the engine.
    orch->resolve_controls(spell, ctx, parent, wrapper);
}

void SpellEngine::execute_components_range(Ref<Spell> spell, Ref<SpellContext> ctx, int start, int end) {
    if (!spell.is_valid() || !ctx.is_valid()) return;
    TypedArray<Ref<SpellComponent>> comps = spell->get_components();
    if (start < 0) start = 0;
    if (end > comps.size()) end = comps.size();
    if (start >= end) return;

    Node *caster_node = ctx->get_caster();
    SpellCaster *sc_for_resolve = nullptr;
    if (caster_node) sc_for_resolve = Object::cast_to<SpellCaster>(caster_node);

    ExecutorRegistry *reg = ExecutorRegistry::get_singleton();

    for (int i = start; i < end; ++i) {
        Ref<SpellComponent> comp = comps[i];
        if (!comp.is_valid()) continue;

        Dictionary resolved = resolve_component_params(comp, Array(), sc_for_resolve, ctx->get_params());
        Dictionary resolved_params;
        if (resolved.has("resolved_params")) resolved_params = resolved["resolved_params"];

        // execute if registered
        String exec_id = comp->get_executor_id();
        if (reg && reg->has_executor(exec_id)) {
            Ref<IExecutor> exec = reg->get_executor(exec_id);
            if (exec.is_valid()) {
                // ensure the resolved params carry a cast id so targets can group events
                cast_counter += 1;
                String cast_id = "spell_cast_" + String::num(cast_counter);
                Dictionary rp_with_cast = resolved_params;
                rp_with_cast["cast_id"] = cast_id;
                exec->execute(ctx, comp, rp_with_cast);
                // If an executor signalled failure through the context, abort further execution
                if (ctx.is_valid()) {
                    Dictionary r = ctx->get_results();
                    if (r.has("executor_failed")) {
                        UtilityFunctions::print(String("SpellEngine: executor '") + comp->get_executor_id() + "' signalled failure; aborting remaining components");
                        return;
                    }
                }
            }
        } else {
            UtilityFunctions::print(String("SpellEngine: no executor registered for: ") + exec_id);
        }
    }
}

void SpellEngine::execute_control_components(Ref<Spell> spell, Ref<SpellContext> ctx) {
    if (!spell.is_valid() || !ctx.is_valid()) return;

    Node *caster_node = ctx->get_caster();
    SpellCaster *sc_for_resolve = nullptr;
    if (caster_node) sc_for_resolve = Object::cast_to<SpellCaster>(caster_node);

    TypedArray<Ref<SpellComponent>> comps = spell->get_components();
    ExecutorRegistry *reg = ExecutorRegistry::get_singleton();

    for (int i = 0; i < comps.size(); ++i) {
        Ref<SpellComponent> comp = comps[i];
        if (!comp.is_valid()) continue;
        String comp_exec_id = comp->get_executor_id();
        // Only execute control-only components here (those that begin with choose_/select_)
        if (!(comp_exec_id.begins_with("choose_") || comp_exec_id.begins_with("select_"))) {
            continue;
        }

        // Resolve params for this component now that ctx may contain control results
        Dictionary resolved = resolve_component_params(comp, Array(), sc_for_resolve, ctx->get_params());
        Dictionary resolved_params;
        if (resolved.has("resolved_params")) resolved_params = resolved["resolved_params"];

        ExecutorRegistry *ereg = ExecutorRegistry::get_singleton();
        if (ereg && ereg->has_executor(comp_exec_id)) {
            Ref<IExecutor> exec = ereg->get_executor(comp_exec_id);
            if (exec.is_valid()) {
                // ensure the resolved params carry a cast id if not already present
                Dictionary rp_with_cast = resolved_params;
                // provide a unique cast id if missing
                cast_counter += 1;
                String cast_id = "spell_cast_" + String::num(cast_counter);
                rp_with_cast["cast_id"] = cast_id;
                exec->execute(ctx, comp, rp_with_cast);
            }
        } else {
            UtilityFunctions::print(String("SpellEngine: no executor registered for control component: ") + comp_exec_id);
        }
    }
}

void SpellEngine::_resolve_controls_forward(const Variant &out, const Variant &orch_v, const Variant &orig_cb_v) {
    // Forward the result to the original callback, then cleanup the orchestrator.
    // orig_cb_v should be a Callable
    if (orig_cb_v.get_type() == Variant::CALLABLE) {
        Callable orig = orig_cb_v;
        Array args;
        args.append(out);
        orig.callv(args);
    }

    // Do NOT delete the orchestrator here: the orchestrator may still be
    // executing on the call stack when this forwarder is invoked. The
    // orchestrator is held by a Ref in resolve_controls and will be released
    // when all references go out of scope. Avoid explicit memdelete here to
    // prevent use-after-free crashes.
}

Array SpellEngine::collect_controls(Ref<Spell> spell, Ref<SpellContext> ctx) {
    Array out;
    if (!spell.is_valid()) return out;

    TypedArray<Ref<SpellComponent>> comps = spell->get_components();
    ExecutorRegistry *reg = ExecutorRegistry::get_singleton();

    for (int i = 0; i < comps.size(); ++i) {
        Ref<SpellComponent> comp = comps[i];
        if (!comp.is_valid()) continue;
        String exec_id = comp->get_executor_id();
        // simple heuristic: control executors are those starting with "choose_" or "select_"
        if (exec_id.begins_with("choose_") || exec_id.begins_with("select_")) {
            Dictionary d;
            d["index"] = i;
            d["executor_id"] = exec_id;
            d["base_params"] = comp->get_base_params();
            // query param schema from registry if available
            if (reg && reg->has_executor(exec_id)) {
                Ref<IExecutor> ex = reg->get_executor(exec_id);
                if (ex.is_valid()) d["param_schema"] = ex->get_param_schema();
            }
            out.append(d);
        }
    }
    return out;
}

bool SpellEngine::validate_control_result(const String &mode, const Dictionary &result) const {
    // Basic server-side sanity checks. Reject NaN, Inf, and values outside reasonable bounds.
    auto is_finite = [&](const Variant &v)->bool{
        if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT) {
            double d = (double)v;
            if (std::isnan(d) || std::isinf(d)) return false;
            return true;
        }
        return true;
    };

    const double MAX_COORD = 1e6;
    const double MAX_RADIUS = 1e4;
    const double MAX_HEIGHT = 1e5;

    if (mode.find("vector") != -1 || mode.find("position") != -1) {
        if (result.has("chosen_vector")) {
            Variant v = result["chosen_vector"];
            if (v.get_type() == Variant::VECTOR3) {
                Vector3 vv = v;
                if (!is_finite(vv.x) || !is_finite(vv.y) || !is_finite(vv.z)) return false;
                if (std::abs(vv.x) > MAX_COORD || std::abs(vv.y) > MAX_COORD || std::abs(vv.z) > MAX_COORD) return false;
            }
        }
        if (result.has("chosen_position")) {
            Variant v = result["chosen_position"];
            if (v.get_type() == Variant::VECTOR3) {
                Vector3 vv = v;
                if (!is_finite(vv.x) || !is_finite(vv.y) || !is_finite(vv.z)) return false;
                if (std::abs(vv.x) > MAX_COORD || std::abs(vv.y) > MAX_COORD || std::abs(vv.z) > MAX_COORD) return false;
            }
        }
    } else if (mode.find("polygon2d") != -1) {
        if (!result.has("chosen_polygon2d")) return false;
        Variant pv = result["chosen_polygon2d"];
        if (pv.get_type() != Variant::ARRAY) return false;
        Array pts = pv;
        if (pts.size() < 3) return false;
        for (int i = 0; i < pts.size(); ++i) {
            Variant p = pts[i];
            if (p.get_type() == Variant::VECTOR2) {
                Vector2 v = p;
                if (!is_finite(v.x) || !is_finite(v.y)) return false;
                if (std::abs(v.x) > MAX_COORD || std::abs(v.y) > MAX_COORD) return false;
            }
        }
    } else if (mode.find("polygon3d") != -1) {
        if (!result.has("chosen_polygon3d")) return false;
        Variant pv = result["chosen_polygon3d"];
        if (pv.get_type() != Variant::ARRAY) return false;
        Array pts = pv;
        if (pts.size() < 3) return false;
        for (int i = 0; i < pts.size(); ++i) {
            Variant p = pts[i];
            if (p.get_type() == Variant::VECTOR3) {
                Vector3 v = p;
                if (!is_finite(v.x) || !is_finite(v.y) || !is_finite(v.z)) return false;
                if (std::abs(v.x) > MAX_COORD || std::abs(v.y) > MAX_COORD || std::abs(v.z) > MAX_COORD) return false;
            }
        }
    } else if (mode == "cylinder") {
        if (!result.has("center")) return false;
        if (!result.has("radius") || !result.has("height")) return false;
        Variant c = result["center"];
        if (c.get_type() != Variant::VECTOR3) return false;
        Vector3 v = c;
        if (!is_finite(v.x) || !is_finite(v.y) || !is_finite(v.z)) return false;
        double r = 0.0, h = 0.0;
        Variant rv = result["radius"];
        Variant hv = result["height"];
        if (rv.get_type() == Variant::INT || rv.get_type() == Variant::FLOAT) r = (double)rv;
        if (hv.get_type() == Variant::INT || hv.get_type() == Variant::FLOAT) h = (double)hv;
        if (!is_finite(r) || !is_finite(h)) return false;
        if (r <= 0.0 || r > MAX_RADIUS) return false;
        if (h <= 0.0 || h > MAX_HEIGHT) return false;
    }

    return true;
}
