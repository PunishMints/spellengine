#include "spellengine/summon_executor.hpp"

#include "spellengine/executor_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include "spellengine/spell_caster.hpp"

using namespace godot;

void SummonExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid()) return;

    // Diagnostic: print resolved_params keys relevant to spawning
    if (resolved_params.has("position_from_caster")) {
        Variant v = resolved_params["position_from_caster"];
        UtilityFunctions::print(String("SummonExecutor: resolved_params.position_from_caster present -> type=") + String::num(v.get_type()));
        if (v.get_type() == Variant::BOOL) UtilityFunctions::print(String("SummonExecutor: position_from_caster=" ) + (Variant(v).operator String()));
    } else {
        UtilityFunctions::print(String("SummonExecutor: resolved_params.position_from_caster NOT present"));
    }
    if (resolved_params.has("offset")) {
        Variant v = resolved_params["offset"];
        UtilityFunctions::print(String("SummonExecutor: resolved_params.offset present -> type=") + String::num(v.get_type()));
        // Print array or vector content
        if (v.get_type() == Variant::VECTOR3) {
            Vector3 vv = v;
            UtilityFunctions::print(String("SummonExecutor: offset Vector3 = ") + Variant(vv).operator String());
        } else if (v.get_type() == Variant::ARRAY) {
            Array a = v;
            UtilityFunctions::print(String("SummonExecutor: offset Array size=") + String::num(a.size()));
        }
    } else {
        UtilityFunctions::print(String("SummonExecutor: resolved_params.offset NOT present"));
    }

    // Prefer a direct PackedScene in 'scene', or a path in 'scene_path'
    Ref<PackedScene> ps;
    if (resolved_params.has("scene") && resolved_params["scene"].get_type() == Variant::OBJECT) {
        Object *o = Object::cast_to<Object>(resolved_params["scene"]);
        PackedScene *p = Object::cast_to<PackedScene>(o);
        if (p) ps = Ref<PackedScene>(p);
    }

    if (!ps.is_valid() && resolved_params.has("scene_path")) {
        Variant spv = resolved_params["scene_path"];
        if (spv.get_type() == Variant::STRING) {
            String path = spv;
            Ref<Resource> r;
            ResourceLoader *rl = ResourceLoader::get_singleton();
            if (rl) r = rl->load(path);
            if (r.is_valid()) {
                PackedScene *p = Object::cast_to<PackedScene>(r.ptr());
                if (p) ps = Ref<PackedScene>(p);
            }
        }
    }

    if (!ps.is_valid()) {
        UtilityFunctions::print(String("SummonExecutor: missing or invalid PackedScene in params for component: ") + component->get_executor_id());
        if (resolved_params.has("scene_path")) {
            Variant spv = resolved_params["scene_path"];
            if (spv.get_type() == Variant::STRING) {
                UtilityFunctions::print(String("SummonExecutor: attempted scene_path = ") + String(spv));
            }
        }
        return;
    }

    // Build transform from params: position (array), rotation (array, Euler radians), scale (array)
    Vector3 pos = Vector3(0,0,0);
    Transform3D t = Transform3D();

    // If caller requested position relative to caster, compute from caster + optional offset
    bool used_caster_position = false;
    if (resolved_params.has("position_from_caster") && resolved_params["position_from_caster"].get_type() == Variant::BOOL && resolved_params["position_from_caster"]) {
        Node *caster_node = ctx->get_caster();
        if (!caster_node) {
            UtilityFunctions::print(String("SummonExecutor: caster_node is NULL when position_from_caster requested"));
        } else {
            // Print the caster's class for debugging
            String ccls = caster_node->get_class();
            UtilityFunctions::print(String("SummonExecutor: caster_node class = ") + ccls);
            Node3D *nc = Object::cast_to<Node3D>(caster_node);
            if (nc) {
                Vector3 caster_pos = nc->get_global_transform().origin;
                UtilityFunctions::print(String("SummonExecutor: caster global origin = ") + Variant(caster_pos).operator String());
                pos = caster_pos;
                used_caster_position = true;
            } else {
                UtilityFunctions::print(String("SummonExecutor: caster_node is not a Node3D (cast failed). Searching ancestors for nearest Node3D."));
                // Walk up the parent chain to find the nearest Node3D (e.g., CharacterBody3D)
                Node *walker = caster_node->get_parent();
                bool found_ancestor = false;
                while (walker) {
                    Node3D *anc = Object::cast_to<Node3D>(walker);
                    if (anc) {
                        Vector3 anc_pos = anc->get_global_transform().origin;
                        UtilityFunctions::print(String("SummonExecutor: found Node3D ancestor class=") + walker->get_class() + String(" origin=") + Variant(anc_pos).operator String());
                        pos = anc_pos;
                        used_caster_position = true;
                        found_ancestor = true;
                        break;
                    }
                    walker = walker->get_parent();
                }
                // If no ancestor Node3D found, try to locate common spawn-point child nodes on the caster
                if (!found_ancestor) {
                    Array candidate_names;
                    candidate_names.append(String("Muzzle"));
                    candidate_names.append(String("MuzzlePoint"));
                    candidate_names.append(String("SpawnPoint"));
                    candidate_names.append(String("Hand"));
                    candidate_names.append(String("WeaponPoint"));
                    for (int ci = 0; ci < candidate_names.size() && !found_ancestor; ++ci) {
                        String nm = candidate_names[ci];
                        NodePath nm_path(nm);
                        if (caster_node->has_node(nm_path)) {
                            Node *cand = caster_node->get_node_or_null(nm_path);
                            if (cand) {
                                Node3D *cand3 = Object::cast_to<Node3D>(cand);
                                if (cand3) {
                                    Vector3 cand_pos = cand3->get_global_transform().origin;
                                    UtilityFunctions::print(String("SummonExecutor: found spawn-point child '") + nm + String("' origin=") + Variant(cand_pos).operator String());
                                    pos = cand_pos;
                                    used_caster_position = true;
                                    found_ancestor = true;
                                    break;
                                }
                            }
                        }
                    }
                }

            // If we determined a caster-based position, apply optional offset here so
            // offsets are handled uniformly regardless of whether the caster itself
            // was a Node3D or we located an ancestor/child spawn point.
            if (used_caster_position && resolved_params.has("offset")) {
                Variant offv = resolved_params["offset"];
                if (offv.get_type() == Variant::ARRAY) {
                    Array off = offv;
                    if (off.size() >= 1) pos.x += (double)off[0];
                    if (off.size() >= 2) pos.y += (double)off[1];
                    if (off.size() >= 3) pos.z += (double)off[2];
                } else if (offv.get_type() == Variant::VECTOR3) {
                    Vector3 vv = offv;
                    pos += vv;
                }
                UtilityFunctions::print(String("SummonExecutor: computed spawn pos after offset = ") + Variant(pos).operator String());
            }
            }
        }
    }

    // Fallback: explicit absolute position array overrides default
    if (!used_caster_position && resolved_params.has("position") && resolved_params["position"].get_type() == Variant::ARRAY) {
        Array parr = resolved_params["position"];
        if (parr.size() >= 1) pos.x = (double)parr[0];
        if (parr.size() >= 2) pos.y = (double)parr[1];
        if (parr.size() >= 3) pos.z = (double)parr[2];
    }

    // rotation: Euler angles in radians [x,y,z]
    Basis b = Basis();
    if (resolved_params.has("rotation") && resolved_params["rotation"].get_type() == Variant::ARRAY) {
        Array rarr = resolved_params["rotation"];
        double rx = 0.0, ry = 0.0, rz = 0.0;
        if (rarr.size() >= 1) rx = (double)rarr[0];
        if (rarr.size() >= 2) ry = (double)rarr[1];
        if (rarr.size() >= 3) rz = (double)rarr[2];
        // Compose rotations: yaw (Y), pitch (X), roll (Z) order: y * x * z
        Quaternion qx = Quaternion(Vector3(1,0,0), rx);
        Quaternion qy = Quaternion(Vector3(0,1,0), ry);
        Quaternion qz = Quaternion(Vector3(0,0,1), rz);
        Quaternion q = qy * qx * qz;
        b = Basis(q);
    }

    // scale
    if (resolved_params.has("scale") && resolved_params["scale"].get_type() == Variant::ARRAY) {
        Array sarr = resolved_params["scale"];
        double sx = 1.0, sy = 1.0, sz = 1.0;
        if (sarr.size() >= 1) sx = (double)sarr[0];
        if (sarr.size() >= 2) sy = (double)sarr[1];
        if (sarr.size() >= 3) sz = (double)sarr[2];
        b.scale(Vector3((float)sx, (float)sy, (float)sz));
    }

    t.basis = b;
    t.origin = pos;

    // Helper: parse a Vector3 from a Variant that may be an ARRAY or VECTOR3
    auto parse_vec3 = [](const Variant &v, const Vector3 &fallback) -> Vector3 {
        if (v.get_type() == Variant::VECTOR3) return Vector3(v);
        if (v.get_type() == Variant::ARRAY) {
            Array a = v;
            double x = 0, y = 0, z = 0;
            if (a.size() >= 1) x = (double)a[0];
            if (a.size() >= 2) y = (double)a[1];
            if (a.size() >= 3) z = (double)a[2];
            return Vector3((float)x, (float)y, (float)z);
        }
        return fallback;
    };

    // Generate spawn positions according to optional pattern parameters.
    Array spawn_positions;
    spawn_positions.append(pos);
    if (resolved_params.has("pattern_type")) {
        String ptype = resolved_params["pattern_type"];
        int count = 1;
        if (resolved_params.has("pattern_count")) {
            Variant pc = resolved_params["pattern_count"];
            if (pc.get_type() == Variant::Type::INT) count = (int)pc;
            else if (pc.get_type() == Variant::Type::FLOAT) count = (int)((double)pc);
        }
        if (count < 1) count = 1;

        if (ptype == String("linear")) {
            double spacing = 1.0;
            if (resolved_params.has("pattern_spacing")) spacing = (double)resolved_params["pattern_spacing"];
            Vector3 dir = parse_vec3(resolved_params.has("pattern_direction") ? resolved_params["pattern_direction"] : Variant(), Vector3(1,0,0));
            if (dir.length() == 0) dir = Vector3(1,0,0);
            dir = dir.normalized();
            Vector3 start_off = parse_vec3(resolved_params.has("pattern_start_offset") ? resolved_params["pattern_start_offset"] : Variant(), Vector3(0,0,0));
            spawn_positions.clear();
            for (int i = 0; i < count; ++i) {
                Vector3 p = pos + start_off + dir * (float)(i * spacing);
                spawn_positions.append(p);
            }
        } else if (ptype == String("circular")) {
            double radius = 1.0;
            if (resolved_params.has("pattern_radius")) radius = (double)resolved_params["pattern_radius"];
            String plane = String("xz");
            if (resolved_params.has("pattern_plane") && resolved_params["pattern_plane"].get_type() == Variant::STRING) plane = resolved_params["pattern_plane"];
            double start_angle = 0.0;
            if (resolved_params.has("pattern_start_angle")) start_angle = (double)resolved_params["pattern_start_angle"];
            spawn_positions.clear();
            for (int i = 0; i < count; ++i) {
                double a = start_angle + (2.0 * Math_PI * (double)i) / (double)count;
                Vector3 off = Vector3();
                if (plane == String("xy")) off = Vector3((float)(radius * cos(a)), (float)(radius * sin(a)), 0.0f);
                else if (plane == String("yz")) off = Vector3(0.0f, (float)(radius * cos(a)), (float)(radius * sin(a)));
                else /* xz */ off = Vector3((float)(radius * cos(a)), 0.0f, (float)(radius * sin(a)));
                Vector3 start_off = parse_vec3(resolved_params.has("pattern_start_offset") ? resolved_params["pattern_start_offset"] : Variant(), Vector3(0,0,0));
                spawn_positions.append(pos + start_off + off);
            }
        } else if (ptype == String("rect" ) || ptype == String("rectangular")) {
            int rows = 1;
            int cols = count;
            if (resolved_params.has("pattern_rows")) {
                Variant pr = resolved_params["pattern_rows"];
                if (pr.get_type() == Variant::Type::INT) rows = (int)pr;
                else if (pr.get_type() == Variant::Type::FLOAT) rows = (int)((double)pr);
            }
            if (resolved_params.has("pattern_columns")) {
                Variant pc2 = resolved_params["pattern_columns"];
                if (pc2.get_type() == Variant::Type::INT) cols = (int)pc2;
                else if (pc2.get_type() == Variant::Type::FLOAT) cols = (int)((double)pc2);
            }
            double spacing = 1.0;
            if (resolved_params.has("pattern_spacing")) spacing = (double)resolved_params["pattern_spacing"];
            Vector3 start_off = parse_vec3(resolved_params.has("pattern_start_offset") ? resolved_params["pattern_start_offset"] : Variant(), Vector3(0,0,0));
            spawn_positions.clear();
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    Vector3 off = Vector3((float)(c * spacing), 0.0f, (float)(r * spacing));
                    spawn_positions.append(pos + start_off + off);
                }
            }
        }
    }
    // Determine spawn_count (pattern may have produced multiple positions)
    int spawn_count = spawn_positions.size();
    if (spawn_count <= 0) spawn_count = 1;

    // Charge additional mana for extra spawns beyond the first. The engine
    // already charged the component's mana_cost once during composition, so
    // we only need to deduct the extra (spawn_count - 1) * mana_cost here.
    double unit_mana = 0.0;
    if (resolved_params.has("mana_cost")) {
        Variant mc = resolved_params["mana_cost"];
        if (mc.get_type() == Variant::INT || mc.get_type() == Variant::FLOAT) unit_mana = (double)mc;
    }
    if (unit_mana <= 0.0) unit_mana = component->get_cost();

    if (spawn_count > 1 && unit_mana > 0.0 && ctx.is_valid()) {
        double extra_total = unit_mana * (double)(spawn_count - 1);
        // Determine aspects to charge: prefer ctx.params.aspects, else caster assigned aspects
        Array aspects_list;
        Dictionary ctx_params = ctx->get_params();
        if (ctx_params.has("aspects") && ctx_params["aspects"].get_type() == Variant::ARRAY) aspects_list = ctx_params["aspects"];
        Node *caster_node = ctx->get_caster();
        SpellCaster *sc = nullptr;
        if (caster_node) sc = Object::cast_to<SpellCaster>(caster_node);
        if (aspects_list.size() == 0 && sc) aspects_list = sc->get_assigned_aspects();

        if (aspects_list.size() == 0) {
            UtilityFunctions::print(String("SummonExecutor: no aspects available to charge mana for multi-spawn; aborting spawn"));
            // Signal abort in context results so callers can handle gracefully
            if (ctx.is_valid()) {
                Dictionary r = ctx->get_results();
                Dictionary err;
                err["executor_id"] = component->get_executor_id();
                err["reason"] = String("no_aspects_for_multi_spawn");
                r["executor_failed"] = err;
                ctx->set_results(r);
            }
            return;
        }

        // Split extra_total evenly among aspects and verify affordability
        double per_aspect = extra_total / (double)aspects_list.size();
        bool ok = true;
        if (sc) {
            for (int ai = 0; ai < aspects_list.size(); ++ai) {
                String a = aspects_list[ai];
                if (!sc->can_deduct(a, per_aspect)) { ok = false; break; }
            }
        } else {
            ok = false;
        }

        if (!ok) {
            UtilityFunctions::print(String("SummonExecutor: caster lacks mana for additional spawns; aborting spawn"));
            // Signal abort reason into context results to avoid leaving callers blind
            if (ctx.is_valid()) {
                Dictionary r = ctx->get_results();
                Dictionary err;
                err["executor_id"] = component->get_executor_id();
                err["reason"] = String("insufficient_mana_for_multi_spawn");
                r["executor_failed"] = err;
                ctx->set_results(r);
            }
            return;
        }

        // Deduct extra mana now
        for (int ai = 0; ai < aspects_list.size(); ++ai) {
            String a = aspects_list[ai];
            sc->deduct_mana(a, per_aspect);
        }
    }

    // Determine parent once
    Node *parent = nullptr;
    Node *caster = ctx->get_caster();
    if (caster) {
        SceneTree *st = caster->get_tree();
        if (st) parent = st->get_current_scene();
        if (!parent) parent = caster->get_parent();
    }

    Array cur_targets = ctx->get_targets();
    Dictionary cur_results = ctx->get_results();
    Array spawned_arr;
    if (cur_results.has("spawned_instances") && cur_results["spawned_instances"].get_type() == Variant::ARRAY) {
        spawned_arr = cur_results["spawned_instances"];
    }

    for (int si = 0; si < spawn_positions.size(); ++si) {
        Vector3 spos = spawn_positions[si];
        // instantiate per-position
        Node *inst = ps->instantiate();
        if (!inst) {
            UtilityFunctions::print(String("SummonExecutor: instantiate() returned null for PackedScene"));
            continue;
        }
        int64_t iid = inst->get_instance_id();
        UtilityFunctions::print(String("SummonExecutor: instantiated PackedScene instance_id=") + Variant(iid).operator String());

        if (parent) {
            String parent_path = String("<no-path>");
            if (parent->has_method("get_path")) parent_path = Variant(parent->get_path()).operator String();
            UtilityFunctions::print(String("SummonExecutor: adding instance to parent: ") + parent_path);
            parent->add_child(inst);
        } else {
            UtilityFunctions::print(String("SummonExecutor: no valid parent found to add instance; skipping add_child"));
        }

        Node3D *n3 = Object::cast_to<Node3D>(inst);
        if (n3) {
            // apply transform per-instance
            Transform3D it = t;
            it.origin = spos;
            UtilityFunctions::print(String("SummonExecutor: instance is Node3D, setting global transform. origin=") + Variant(spos).operator String());
            n3->set_global_transform(it);
        }

        // Add to context lists
        cur_targets.append(inst);
        spawned_arr.append(inst);

        // inert handling per-instance
        inst->set_meta(String("spell_inert"), Variant(true));
        RigidBody3D *rb = Object::cast_to<RigidBody3D>(inst);
        if (rb) {
            int prev_mode = 0;
            Variant prev_mode_v = rb->get("mode");
            if (prev_mode_v.get_type() == Variant::Type::INT) prev_mode = (int)prev_mode_v;
            inst->set_meta(String("spell_prev_mode"), Variant(prev_mode));
            rb->set("mode", Variant(1));
            rb->set("freeze", Variant(true));
        } else {
            inst->set_physics_process(false);
        }
    }

    // Write back targets/results
    ctx->set_targets(cur_targets);
    cur_results["spawned_instances"] = spawned_arr;
    if (spawned_arr.size() > 0) cur_results["last_spawned"] = spawned_arr[spawned_arr.size() - 1];
    ctx->set_results(cur_results);
    UtilityFunctions::print(String("SummonExecutor: appended spawned instances to ctx.targets and ctx.results; targets_size=") + String::num(cur_targets.size()));
}

void SummonExecutor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_param_schema"), &SummonExecutor::get_param_schema);
}

String SummonExecutor::get_executor_id() const {
    return String("summon_scene_v1");
}

Dictionary SummonExecutor::get_param_schema() const {
    Dictionary schema;
    Dictionary e;

    e = Dictionary();
    e["type"] = "string";
    e["default"] = String("");
    e["desc"] = String("Path to a PackedScene resource to instantiate (res://...)");
    schema["scene_path"] = e;

    e = Dictionary();
    e["type"] = "object";
    e["default"] = Variant();
    e["desc"] = String("Optional PackedScene resource directly supplied");
    schema["scene"] = e;

    e = Dictionary();
    e["type"] = "array";
    Array defpos; defpos.append(0.0); defpos.append(0.0); defpos.append(0.0);
    e["default"] = defpos;
    e["desc"] = String("Position (x,y,z) to place the instance");
    schema["position"] = e;

    e = Dictionary();
    e["type"] = "bool";
    e["default"] = false;
    e["desc"] = String("If true, compute position from the caster's global transform and apply optional 'offset' array");
    schema["position_from_caster"] = e;

    e = Dictionary();
    e["type"] = "array";
    Array defoff; defoff.append(0.0); defoff.append(2.0); defoff.append(0.0);
    e["default"] = defoff;
    e["desc"] = String("Optional offset [x,y,z] to apply when using position_from_caster");
    schema["offset"] = e;

    e = Dictionary();
    e["type"] = "array";
    Array defrot; defrot.append(0.0); defrot.append(0.0); defrot.append(0.0);
    e["default"] = defrot;
    e["desc"] = String("Rotation Euler angles (radians) [x,y,z]");
    schema["rotation"] = e;

    e = Dictionary();
    e["type"] = "array";
    Array defscale; defscale.append(1.0); defscale.append(1.0); defscale.append(1.0);
    e["default"] = defscale;
    e["desc"] = String("Scale (x,y,z)");
    schema["scale"] = e;

    return schema;
}

// Register factory for automatic registration at module init
REGISTER_EXECUTOR_FACTORY(SummonExecutor)
