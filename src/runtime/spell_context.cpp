#include "spellengine/spell_context.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "spellengine/spell.hpp"
#include <algorithm>
#include <vector>

using namespace godot;

void SpellContext::set_caster(Node *p_caster) {
    caster = p_caster;
}

Node *SpellContext::get_caster() const {
    return caster;
}

void SpellContext::set_targets(const Array &p_targets) {
    targets = p_targets;
}

Array SpellContext::get_targets() const {
    return targets;
}

void SpellContext::set_params(const Dictionary &p_params) {
    params = p_params;
}

Dictionary SpellContext::get_params() const {
    return params;
}

void SpellContext::set_results(const Dictionary &p_results) {
    results = p_results;
}

Dictionary SpellContext::get_results() const {
    return results;
}

Dictionary SpellContext::derive_aspect_distribution(Ref<Spell> spell) const {
    Dictionary out;
    if (!spell.is_valid()) return out;

    TypedArray<Ref<SpellComponent>> comps = spell->get_components();
    double total = 0.0;
    Dictionary sums;

    for (int i = 0; i < comps.size(); ++i) {
        Ref<SpellComponent> comp = comps[i];
        if (!comp.is_valid()) continue;
        Dictionary contribs = comp->get_aspects_contributions();
        Array keys = contribs.keys();
        for (int k = 0; k < keys.size(); ++k) {
            String a = keys[k];
            Variant v = contribs[a];
            if (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) {
                double val = (double)v;
                if (sums.has(a)) sums[a] = (double)sums[a] + val;
                else sums[a] = val;
                total += val;
            }
        }
    }

    if (total <= 0.0) return out;

    Array sk = sums.keys();
    for (int i = 0; i < sk.size(); ++i) {
        String a = sk[i];
        double v = (double)sums[a];
        out[a] = v / total;
    }

    return out;
}

Array SpellContext::derive_aspects_list(Ref<Spell> spell) const {
    Dictionary dist = derive_aspect_distribution(spell);
    Array out;
    if (dist.size() == 0) return out;

    Array keys = dist.keys();

    struct Pair { String a; double w; };
    std::vector<Pair> vec;
    vec.reserve(keys.size());
    for (int i = 0; i < keys.size(); ++i) {
        String a = keys[i];
        double w = (double)dist[a];
        vec.push_back(Pair{a, w});
    }
    std::sort(vec.begin(), vec.end(), [](const Pair &p1, const Pair &p2){ return p1.w > p2.w; });

    for (size_t i = 0; i < vec.size(); ++i) out.push_back(vec[i].a);
    return out;
}

void SpellContext::derive_and_set_aspects(Ref<Spell> spell) {
    Array list = derive_aspects_list(spell);
    Dictionary p = get_params();
    p["aspects"] = list;
    set_params(p);
}

void SpellContext::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_caster", "caster"), &SpellContext::set_caster);
    ClassDB::bind_method(D_METHOD("get_caster"), &SpellContext::get_caster);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "caster", PROPERTY_HINT_RESOURCE_TYPE, "Node"), "set_caster", "get_caster");

    ClassDB::bind_method(D_METHOD("set_targets", "targets"), &SpellContext::set_targets);
    ClassDB::bind_method(D_METHOD("get_targets"), &SpellContext::get_targets);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "targets"), "set_targets", "get_targets");

    ClassDB::bind_method(D_METHOD("set_params", "params"), &SpellContext::set_params);
    ClassDB::bind_method(D_METHOD("get_params"), &SpellContext::get_params);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "params"), "set_params", "get_params");

    ClassDB::bind_method(D_METHOD("set_results", "results"), &SpellContext::set_results);
    ClassDB::bind_method(D_METHOD("get_results"), &SpellContext::get_results);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "results"), "set_results", "get_results");

    ClassDB::bind_method(D_METHOD("derive_aspect_distribution", "spell"), &SpellContext::derive_aspect_distribution);
    ClassDB::bind_method(D_METHOD("derive_aspects_list", "spell"), &SpellContext::derive_aspects_list);
    ClassDB::bind_method(D_METHOD("derive_and_set_aspects", "spell"), &SpellContext::derive_and_set_aspects);
}
