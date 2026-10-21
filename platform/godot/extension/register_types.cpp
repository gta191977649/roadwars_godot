#include "platform/godot/extension/game_host.hpp"
#include <godot_cpp/godot.hpp>

namespace roadwars {
void initialize(godot::ModuleInitializationLevel level) {
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        godot::ClassDB::register_class<OverlayLogger>();
        godot::ClassDB::register_class<GameHost>();
    }
}
void uninitialize(godot::ModuleInitializationLevel) {}
} // namespace roadwars
extern "C" {
GDExtensionBool GDE_EXPORT roadwars_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization) {
    try {
        godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
        init.register_initializer(roadwars::initialize);
        init.register_terminator(roadwars::uninitialize);
        init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
        return init.init();
    } catch (...) {
        return false;
    }
}
}
