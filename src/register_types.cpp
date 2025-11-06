// Include your classes, that you want to expose to Godot
#include "spellengine/item_data.hpp"
#include "spellengine/aspect.hpp"
#include "spellengine/spell_component.hpp"
#include "spellengine/executor_registry.hpp"
#include "spellengine/spell_context.hpp"
#include "spellengine/spell_template.hpp"
#include "spellengine/spell.hpp"
#include "spellengine/spell_engine.hpp"
#include "spellengine/executor_registry.hpp"
#include "spellengine/executor_base.hpp"
#include "spellengine/damage_executor.hpp"
#include "spellengine/dot_executor.hpp"
#include "spellengine/knockback_executor.hpp"
#include "spellengine/status_effect.hpp"
#include "spellengine/spell_caster.hpp"
#include "spellengine/synergy_registry.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/project_settings.hpp>

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level)
{
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Register your classes here, so they are available in the Godot editor and engine
	GDREGISTER_CLASS(ItemData)
	GDREGISTER_CLASS(Aspect)
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
	GDREGISTER_CLASS(StatusEffect)
	GDREGISTER_CLASS(SpellCaster)
	GDREGISTER_CLASS(SynergyRegistry)

	// Create and register default executors
	ExecutorRegistry *reg = ExecutorRegistry::get_singleton();
	if (reg) {
		Ref<IExecutor> d = memnew(DamageExecutor);
		reg->register_executor("damage_v1", d);

		Ref<IExecutor> dot = memnew(DotExecutor);
		reg->register_executor("dot_v1", dot);

		Ref<IExecutor> kb = memnew(KnockbackExecutor);
		reg->register_executor("knockback_v1", kb);
	}

	// Export executor ids and parameter schemas to ProjectSettings so editor plugins can query them
	Array exec_ids;
	Dictionary exec_schemas;
	if (reg) {
		Array ids = reg->get_executor_ids();
		for (int i = 0; i < ids.size(); ++i) {
			String id = ids[i];
			exec_ids.append(id);
			Ref<IExecutor> ex = reg->get_executor(id);
			if (ex.is_valid()) {
				Dictionary schema = ex->get_param_schema();
				exec_schemas[id] = schema;
			}
		}
	}
	ProjectSettings::get_singleton()->set_setting("spellengine/executor_ids", exec_ids);
	ProjectSettings::get_singleton()->set_setting("spellengine/executor_schemas", exec_schemas);

	// Diagnostic prints so the editor log shows what the module exported.
	UtilityFunctions::print(String("[spellengine] exported executor_ids count=") + String::num_int64(exec_ids.size()));
	for (int i = 0; i < exec_ids.size(); ++i) {
		UtilityFunctions::print(String("[spellengine] executor_id: ") + String(exec_ids[i]));
	}
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
