#include "spellengine/spell_engine.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "spellengine/executor_registry.hpp"
#include "spellengine/executor_base.hpp"
#include "spellengine/spell_caster.hpp"
#include "spellengine/synergy_registry.hpp"
#include <godot_cpp/variant/callable.hpp>

using namespace godot;

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

void SpellEngine::execute_spell(Ref<Spell> spell, Ref<SpellContext> ctx) {
    if (!spell.is_valid()) {
        UtilityFunctions::print("SpellEngine: invalid spell passed to execute_spell");
        return;
    }

    Array casting_aspects;
    Dictionary ctx_params = ctx->get_params();
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

        Dictionary resolved = resolve_component_params(comp, casting_aspects, sc_for_resolve);
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
        if (reg && reg->has_executor(comp->get_executor_id())) {
            Ref<IExecutor> exec = reg->get_executor(comp->get_executor_id());
            if (exec.is_valid()) {
                exec->execute(ctx, comp, resolved_params);
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

            SynergyRegistry *sreg = SynergyRegistry::get_singleton();
            if (sreg && sreg->has_synergy(skey)) {
                Dictionary spec = sreg->get_synergy(skey);
                if (spec.has("callable")) {
                    Variant cv = spec["callable"];
                    if (cv.get_type() == Variant::CALLABLE) {
                        Callable cb = cv;
                        Array args;
                        args.push_back(ctx);
                        args.push_back(comp);
                        args.push_back(resolved_params);
                        args.push_back(spec);
                        args.push_back(skey);
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
                                args.push_back(resolved_params);
                                args.push_back(spec);
                                args.push_back(skey);
                                cb.callv(args);
                            }
                        }
                    }
                }
                if (spec.has("extra_executors")) {
                    Variant ev = spec["extra_executors"];
                    if (ev.get_type() == Variant::ARRAY) {
                        Array extras = ev;
                        for (int ei = 0; ei < extras.size(); ++ei) {
                            Variant exv = extras[ei];
                            if (exv.get_type() != Variant::DICTIONARY) continue;
                            Dictionary exd = exv;

                            if (exd.has("trigger_on_executor")) {
                                Variant tov = exd["trigger_on_executor"];
                                if (tov.get_type() == Variant::STRING) {
                                    String trig = tov;
                                    if (trig != comp->get_executor_id()) continue;
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
                                if (extra_exec.is_valid()) extra_exec->execute(ctx, comp, extra_params);
                            }
                        }
                    }
                }
            }
        }
    }
}

Dictionary SpellEngine::resolve_component_params(Ref<SpellComponent> component, const Array &casting_aspects, SpellCaster *caster) {
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
                acc += share * (base_num * mod);
            }
            double final_val = acc;
            if (caster) {
                double mult = 0.0;
                for (int i = 0; i < aspects_used.size(); ++i) {
                    String a = aspects_used[i];
                    double share = (double)normalized[a];
                    double scaler = caster->get_scaler(a, (String)key);
                    mult += share * scaler;
                }
                if (mult <= 0.0) mult = 1.0;
                final_val = acc * mult;
            }
            resolved_params[key] = final_val;
        } else {
            resolved_params[key] = base_val;
        }
    }

    double total_cost = component->get_cost();
    if (resolved_params.has("mana_cost")) {
        Variant mc = resolved_params["mana_cost"];
        if (mc.get_type() == Variant::INT || mc.get_type() == Variant::FLOAT) total_cost = (double)mc;
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

        if (synergy_mods.size() > 0 && synergy_mods.has(key)) {
            Variant sv = synergy_mods[key];
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
        if (sreg && sreg->has_synergy(key)) {
            Dictionary spec = sreg->get_synergy(key);
            if (spec.has("modifiers")) {
                Variant mv = spec["modifiers"];
                if (mv.get_type() == Variant::DICTIONARY) {
                    Dictionary md = mv;
                    Array mk = md.keys();
                    for (int i = 0; i < mk.size(); ++i) {
                        String mkey = mk[i];
                        resolved_params[mkey] = md[mkey];
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
    }

    out["resolved_params"] = resolved_params;
    out["cost_per_aspect"] = cost_per_aspect;
    out["aspects_used"] = aspects_used;
    return out;
}

void SpellEngine::_bind_methods() {
    ClassDB::bind_method(D_METHOD("build_spell_from_aspects", "aspects"), &SpellEngine::build_spell_from_aspects);
    ClassDB::bind_method(D_METHOD("execute_spell", "spell", "context"), &SpellEngine::execute_spell);
}
