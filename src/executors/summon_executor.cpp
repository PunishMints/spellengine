#include "spellengine/summon_executor.hpp"

#include "spellengine/executor_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>

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

    // Instantiate and add to scene. Prefer caster's current scene if available.
    Node *inst = ps->instantiate();
    if (!inst) {
        UtilityFunctions::print(String("SummonExecutor: instantiate() returned null for PackedScene"));
    } else {
        // Print the instance id for debugging
    int64_t iid = inst->get_instance_id();
    UtilityFunctions::print(String("SummonExecutor: instantiated PackedScene instance_id=") + Variant(iid).operator String());
    }
    if (!inst) {
        UtilityFunctions::print(String("SummonExecutor: instantiate() returned null for PackedScene"));
        return;
    }

    Node *parent = nullptr;
    Node *caster = ctx->get_caster();
    if (caster) {
        SceneTree *st = caster->get_tree();
        if (st) parent = st->get_current_scene();
        // If current_scene is null, fall back to caster's parent in the scene tree
        if (!parent) parent = caster->get_parent();
    }

    if (parent) {
        String parent_path = String("<no-path>");
        if (parent->has_method("get_path")) {
            parent_path = Variant(parent->get_path()).operator String();
        }
        UtilityFunctions::print(String("SummonExecutor: adding instance to parent: ") + parent_path);
        parent->add_child(inst);
    } else {
        UtilityFunctions::print(String("SummonExecutor: no valid parent found to add instance; skipping add_child"));
    }

    // If the instance is a Node3D, set its global transform
    Node3D *n3 = Object::cast_to<Node3D>(inst);
    if (n3) {
        UtilityFunctions::print(String("SummonExecutor: instance is Node3D, setting global transform. origin=") + Variant(pos).operator String());
        n3->set_global_transform(t);
    }

    // Add the spawned instance to the SpellContext so subsequent components
    // (e.g. a Force executor) or synergy extra_executors can access it via
    // ctx->get_targets() or ctx->get_results(). We append to targets and also
    // record in results.spawned_instances and results.last_spawned.
    if (ctx.is_valid()) {
        Array cur_targets = ctx->get_targets();
        cur_targets.append(inst);
        ctx->set_targets(cur_targets);

        Dictionary cur_results = ctx->get_results();
        Array spawned_arr;
        if (cur_results.has("spawned_instances") && cur_results["spawned_instances"].get_type() == Variant::ARRAY) {
            spawned_arr = cur_results["spawned_instances"];
        }
        spawned_arr.append(inst);
        cur_results["spawned_instances"] = spawned_arr;
        cur_results["last_spawned"] = inst;
        // Mark the spawned instance as inert until a later component (e.g. Force)
        // activates it. This prevents the projectile from immediately being
        // simulated/affected by physics before the player confirms a control.
        // We store a simple meta flag 'spell_inert' and disable physics processing
        // on the node where possible.
        inst->set_meta(String("spell_inert"), Variant(true));
        // Try to disable physics processing on the instance so it remains inert.
        // This is a best-effort action; if the instance is a RigidBody3D its
        // internal physics may still run, but many custom projectiles will
        // respect physics_process being disabled on the root node.
        inst->set_physics_process(false);
        ctx->set_results(cur_results);

        UtilityFunctions::print(String("SummonExecutor: appended spawned instance to ctx.targets and ctx.results; targets_size=") + String::num(cur_targets.size()));
    }
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
