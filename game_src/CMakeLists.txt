add_library(roadwars_core STATIC core/runtime.cpp core/graphics_settings.cpp unresolved/game_entry.cpp
  unresolved/menu_frontend.cpp unresolved/frontend_canvas.cpp unresolved/frontend_custom.cpp
  unresolved/frontend_video.cpp unresolved/frontend_localization.cpp
  unresolved/frontend_scene.cpp unresolved/chrome_mapping.cpp
  unresolved/race_frontend.cpp unresolved/cockpit_frontend.cpp
  unresolved/race_frontend_equipment.cpp unresolved/skeletal_mesh_runtime.cpp
  unresolved/race_music.cpp unresolved/race_start.cpp
  unresolved/race_loadout.cpp
  unresolved/vehicle_equipment_effects.cpp unresolved/race_ai_combat.cpp
  unresolved/weapon_runtime.cpp unresolved/turret_runtime.cpp
  unresolved/race_turrets.cpp unresolved/weapon_projectiles.cpp
  unresolved/rear_weapon_effects.cpp
  unresolved/guided_weapon_runtime.cpp
  unresolved/special_weapon_effects.cpp
  unresolved/contact_weapon_runtime.cpp unresolved/rear_weapon_instances.cpp
  unresolved/scene_turret_combat.cpp unresolved/weapon_trails.cpp unresolved/vehicle_combat.cpp
  unresolved/weapon_rear_blasts.cpp
  unresolved/race_callback_registry.cpp unresolved/combat_audio.cpp
  unresolved/scene_electron_traps.cpp
  unresolved/scene_weather.cpp
  unresolved/vehicle_damage_particles.cpp unresolved/chimney_particles.cpp
  unresolved/vehicle_damage_visuals.cpp
  unresolved/race_route.cpp unresolved/race_ai.cpp unresolved/race_session.cpp unresolved/race_collision.cpp
  unresolved/resource_store.cpp unresolved/mesh_scene.cpp unresolved/bitmap.cpp
  unresolved/track_collision.cpp unresolved/game_initialization.cpp
  unresolved/race_presentation.cpp unresolved/vehicle_dynamics.cpp unresolved/vehicle_input.cpp
  unresolved/contact_orientation.cpp unresolved/wall_response.cpp
  unresolved/track_render_settings.cpp unresolved/particle_effects.cpp unresolved/wheel_pose.cpp
  unresolved/engine_audio.cpp unresolved/track_animation.cpp)
include(${CMAKE_CURRENT_SOURCE_DIR}/generated_classes.cmake OPTIONAL)
target_sources(roadwars_core PRIVATE unresolved/skid_marks.cpp unresolved/original_camera.cpp unresolved/weapon_mounts.cpp)
if(ROADWARS_CLASS_SOURCES)
  target_sources(roadwars_core PRIVATE ${ROADWARS_CLASS_SOURCES})
endif()
target_include_directories(roadwars_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(roadwars_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(roadwars_core PRIVATE /W4 /fp:strict)
else()
  target_compile_options(roadwars_core PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

target_sources(roadwars_core PRIVATE unresolved/collision_audio.cpp unresolved/tire_audio.cpp)
