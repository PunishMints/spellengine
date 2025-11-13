#include "spellengine/control_manager.hpp"
#include "spellengine/control_gizmo.hpp"
#include "spellengine/spell_engine.hpp"
#include "spellengine/control_input_controller.hpp"
#include "spellengine/control_preview_factory.hpp"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

ControlManager::ControlManager() {
}

ControlManager::~ControlManager() {
}

void ControlManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_gizmo", "mode", "preview_scene"), &ControlManager::create_gizmo, DEFVAL(Ref<PackedScene>()));
    ClassDB::bind_method(D_METHOD("attach_and_start", "parent", "gizmo_node", "user_completion", "mode"), &ControlManager::attach_and_start, DEFVAL(String()));
    // Note: gizmo_obj is the first arg because the Callable we bind prepends
    // the gizmo Variant. Match the bound argument ordering here.
    ClassDB::bind_method(D_METHOD("_on_gizmo_complete", "gizmo_obj", "result"), &ControlManager::_on_gizmo_complete);
}

Node *ControlManager::create_gizmo(const String &mode, const Ref<PackedScene> &preview_scene) {
    ControlGizmo *g = memnew(ControlGizmo());
    g->set_mode(mode);
    if (preview_scene.is_valid()) {
        g->set_preview_scene(preview_scene);
    }
    return g;
}

void ControlManager::attach_and_start(Node *parent, Node *gizmo_node, const Callable &user_completion, const String &mode) {
    if (!parent || !gizmo_node) {
        return;
    }

    // Add to parent in scene tree
    parent->add_child(gizmo_node);

    // Cast and configure
    ControlGizmo *gizmo = Object::cast_to<ControlGizmo>(gizmo_node);
    if (!gizmo) {
        return;
    }

    // Ensure the gizmo will receive per-frame _process() calls. Some
    // environments may add gizmos while processing is disabled on the
    // node; explicitly enable processing here so gizmo hover polling runs.
    gizmo->set_process(true);
    gizmo->set_process_input(true);

    int64_t id = gizmo->get_instance_id();

    gizmo_user_callable[id] = Variant(user_completion);
    gizmo_mode_map[id] = Variant(mode);

    // Set the gizmo's completion callable to call our handler. We do NOT bind
    // the gizmo instance here; instead the gizmo will pass itself as the
    // first argument when invoking the callable. This avoids lifetime/order
    // issues where a bound Variant may become null.
    Callable c = Callable(this, "_on_gizmo_complete");
    gizmo->set_completion_callable(c);

    // Ensure we have an input controller attached to the parent to route input
    // If the caller provided a camera on the parent (e.g., caster camera), it
    // should be set on the input controller via ensure_input_controller.
    godot::Camera3D *use_cam = nullptr;
    // Try to find a Camera3D child on parent
    if (parent) {
        // Try to find a Camera3D named "Camera3D" under parent; fallback to viewport camera
        Node *maybe = parent->get_node_or_null(String("Camera3D"));
        if (maybe) use_cam = Object::cast_to<Camera3D>(maybe);
    }
    ensure_input_controller(parent, use_cam);
    if (input_controller) {
        // register the gizmo so input events will be routed to it
        input_controller->add_gizmo(gizmo);
    }

    // If no explicit preview scene was provided on the gizmo, ask the preview
    // factory to create a lightweight built-in preview instance and attach it.
    if (!gizmo->get_preview_scene().is_valid()) {
        ensure_preview_factory();
        if (preview_factory) {
            Node *pv = preview_factory->create_preview(mode);
            if (pv) gizmo->set_preview_instance(pv);
        }
    }
}

void ControlManager::ensure_input_controller(Node *parent, godot::Camera3D *camera) {
    if (input_controller) return;
    if (!parent) return;
    input_controller = memnew(ControlInputController());
    parent->add_child(input_controller);
    if (camera) input_controller->set_camera(camera);
}

void ControlManager::ensure_preview_factory() {
    if (preview_factory) return;
    preview_factory = memnew(ControlPreviewFactory());
}

void ControlManager::_on_gizmo_complete(godot::Object *gizmo_obj, const godot::Variant &result) {
    // Identify gizmo by instance id
    if (!gizmo_obj) {
        UtilityFunctions::print("[ControlManager] _on_gizmo_complete called with null gizmo_obj");
        return;
    }
    int64_t id = gizmo_obj->get_instance_id();
    UtilityFunctions::print(String("[ControlManager] _on_gizmo_complete enter gizmo_id=") + String::num_int64(id));

    Variant mode_v = gizmo_mode_map.get(Variant(id), Variant());
    Variant user_cb_v = gizmo_user_callable.get(Variant(id), Variant());

    String mode = String();
    if (mode_v.get_type() == Variant::Type::STRING) {
        mode = mode_v;
    }

    // Basic validation via SpellEngine utility (use temporary instance - validation is stateless)
    SpellEngine *se = memnew(SpellEngine());
    bool ok = false;
    if (result.get_type() == Variant::DICTIONARY) {
        Dictionary dict = result;
        ok = se->validate_control_result(mode, dict);
    }

    // Call user callback if provided. Pass a dictionary with keys {"ok":bool, "result":Dictionary}
    if (user_cb_v.get_type() == Variant::Type::CALLABLE) {
        Callable user_cb = user_cb_v;
        Array args;
        Dictionary out;
        out["ok"] = ok;
        out["result"] = result;
        args.append(out);
        UtilityFunctions::print(String("[ControlManager] calling user callback for gizmo_id=") + String::num_int64(id));
        user_cb.callv(args);
        UtilityFunctions::print(String("[ControlManager] returned from user callback for gizmo_id=") + String::num_int64(id));
    }

    // Clean up maps
    gizmo_user_callable.erase(id);
    gizmo_mode_map.erase(id);

    // Optionally remove gizmo from scene tree
    Node *n = Object::cast_to<Node>(gizmo_obj);
    if (n) {
        // Set a short-lived ignore-until meta on the parent so scripts that
        // poll Input.is_action_just_pressed can ignore the confirming click.
        // Use the engine-monotonic Time singleton (ticks in ms) so this value
        // is stable and comparable from GDScript via Time.get_ticks_msec().
        Node *n_parent = n->get_parent();
        if (n_parent) {
            int64_t now_ms = (int64_t)Time::get_singleton()->get_ticks_msec();
            // ignore actions for 200ms
            n_parent->set_meta(String("spellengine_ignore_actions_until"), Variant(now_ms + (int64_t)200));
        }
        // unregister from input controller so it can restore cursor/input state
        if (input_controller) {
            input_controller->remove_gizmo(n);
            // If the input controller no longer manages any gizmos, remove it as
            // well so we don't leave stale controllers in the scene. This mirrors
            // the gizmo cleanup above and ensures input controllers are cleaned
            // up when no longer needed.
            if (input_controller->get_gizmo_count() == 0) {
                Node *ctrl_parent = input_controller->get_parent();
                if (ctrl_parent) {
                    ctrl_parent->remove_child(input_controller);
                }
                input_controller->queue_free();
                input_controller = nullptr;
            }
        }
        if (n->get_parent()) {
            n->get_parent()->remove_child(n);
        }
        // queue free to let Godot cleanup the node. This is safe because
        // we removed it from the parent and unregistered it from the
        // input controller above; queue_free defers actual deletion until
        // the engine's idle loop so we avoid immediate use-after-free.
        n->queue_free();
    }

    memdelete(se);
    UtilityFunctions::print(String("[ControlManager] _on_gizmo_complete exit gizmo_id=") + String::num_int64(id));
}
