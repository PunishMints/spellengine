// Include your classes, that you want to expose to Godot
#include "spellengine/item_data.hpp"
#include "spellengine/aspect.hpp"
#include "spellengine/spell_component.hpp"
#include "spellengine/executor_registry.hpp"
#include "spellengine/spell_context.hpp"
#include "spellengine/spell_template.hpp"
#include "spellengine/spell.hpp"
#include "spellengine/spell_engine.hpp"
#include "spellengine/aspect_registry.hpp"
#include "spellengine/editor/aspect_registry_plugin.hpp"
#include "spellengine/spell_component_registry.hpp"
#include "spellengine/editor/spell_component_registry_plugin.hpp"
#include "spellengine/executor_registry.hpp"
#include "spellengine/executor_base.hpp"
#include "spellengine/damage_executor.hpp"
#include "spellengine/dot_executor.hpp"
#include "spellengine/knockback_executor.hpp"
#include "spellengine/summon_executor.hpp"
#include "spellengine/force_executor.hpp"
#include "spellengine/status_effect.hpp"
#include "spellengine/spell_caster.hpp"
#include "spellengine/synergy_registry.hpp"
#include "spellengine/synergy.hpp"
#include "spellengine/control_gizmo.hpp"
#include "spellengine/control_manager.hpp"
#include "spellengine/control_orchestrator.hpp"
#include "spellengine/control_input_controller.hpp"
#include "spellengine/control_preview_factory.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Register your classes here, so they are available in the Godot editor and engine
    GDREGISTER_CLASS(ItemData)
    GDREGISTER_CLASS(Aspect)
    GDREGISTER_CLASS(AspectRegistry)
    GDREGISTER_CLASS(SpellComponentRegistry)
    GDREGISTER_CLASS(SpellComponent)
    GDREGISTER_CLASS(ExecutorRegistry)
    GDREGISTER_CLASS(SpellContext)
    GDREGISTER_CLASS(SpellTemplate)
    GDREGISTER_CLASS(Spell)
    GDREGISTER_CLASS(SpellEngine)
    GDREGISTER_ABSTRACT_CLASS(IExecutor)
    GDREGISTER_CLASS(DamageExecutor)
    GDREGISTER_CLASS(DotExecutor)
    GDREGISTER_CLASS(KnockbackExecutor)
    GDREGISTER_CLASS(SummonExecutor)
    GDREGISTER_CLASS(ForceExecutor)
    GDREGISTER_CLASS(StatusEffect)
    GDREGISTER_CLASS(SpellCaster)
    GDREGISTER_CLASS(ControlGizmo)
    GDREGISTER_CLASS(ControlManager)
    GDREGISTER_CLASS(ControlOrchestrator)
    GDREGISTER_CLASS(ControlInputController)
    GDREGISTER_CLASS(ControlPreviewFactory)
    GDREGISTER_CLASS(Synergy)
    GDREGISTER_CLASS(SynergyRegistry)

    // Ensure AspectRegistry singleton exists and attempt to populate from res://aspects
    AspectRegistry *areg = AspectRegistry::get_singleton();
    if (areg) {
        areg->load_all_from_dir("res://aspects", true);
    }

    // Ensure SpellComponentRegistry exists and populate from res://spell_components
    SpellComponentRegistry *sreg = SpellComponentRegistry::get_singleton();
    if (sreg) {
        sreg->load_all_from_dir("res://spell_components", true);
    }

    // Ensure SynergyRegistry exists and attempt to populate from res://synergies
    SynergyRegistry *synreg = SynergyRegistry::get_singleton();
    if (synreg) {
        int loaded = synreg->load_all_from_dir("res://synergies", true);
        using namespace godot;
        UtilityFunctions::print(String("[SpellEngine] loaded synergies: ") + String::num(loaded));
        Array keys = synreg->get_synergy_keys();
        for (int ki = 0; ki < keys.size(); ++ki) {
            Variant kv = keys[ki];
            String key = (kv.get_type() == Variant::STRING) ? (String)kv : String(kv);
            Dictionary spec = synreg->get_synergy(key);
            UtilityFunctions::print(String("  - ") + key);
            UtilityFunctions::print(spec);
        }
    }

    // Register editor plugin (available to add in the editor)
    // Only register EditorPlugin-derived classes when the EditorPlugin base exists
    if (ClassDB::class_exists("EditorPlugin")) {
        GDREGISTER_CLASS(AspectRegistryEditorPlugin)
        GDREGISTER_CLASS(SpellComponentRegistryEditorPlugin)
    }

    // Register all executor factories that registered themselves using
    // REGISTER_EXECUTOR_FACTORY(...) in their cpp files. This removes the need
    // to hardcode executor ids or instances here.
    ExecutorRegistry::register_all_factories();
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C"
{
    // Initialization
    GDExtensionBool GDE_EXPORT spellengine_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
    {
        GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
        init_obj.register_initializer(initialize_gdextension_types);
        init_obj.register_terminator(uninitialize_gdextension_types);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}
