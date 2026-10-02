"""Places one ANightlightSceneryScatter in /Game/NightLightV2 and fills it.

The scatter is pointed at the placed world generator and given the desert rocks and
the desert mountains. Its spacing and scale settings keep their C++ defaults, so they
are tuned on the placed actor. It also lays a wide ground plane just under the map, above
the level's flat grid landscape, with the pack's dark studio floor material, so the gap
between the map and the mountains no longer shows the grid.

Run with the editor closed, after nightlight_terrain_material (it lifts the generator):
UnrealEditor-Cmd.exe "<absolute path>/NightLightV2.uproject" -run=pythonscript "-script=__import__('nightlight_scenery')"
Running it again updates the scatter and ground already in the level instead of adding more.
"""
import unreal

DESERT = "/Game/Desert_Castle/Desert_Castle_Vol1/Meshes/"
ROCK_MESHES = [DESERT + "SM_Desert_Rock_02", DESERT + "SM_Desert_Rock_03", DESERT + "SM_Desert_Rock_04"]
BORDER_MESHES = [DESERT + "SM_Desert_Mountain_01", DESERT + "SM_Desert_Mountain_03"]

# Level actors are found and spawned through the editor actor subsystem (Epic Games, Inc., 2026a).
unreal.EditorLoadingAndSavingUtils.load_map("/Game/NightLightV2")
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# World Partition only loads actors near the editor camera, so load the generator and
# any scatter or ground from an earlier run (Epic Games, Inc., 2026b).
wanted = [unreal.NightlightWorldGenerator.static_class(), unreal.NightlightSceneryScatter.static_class()]
unreal.WorldPartitionBlueprintLibrary.load_actors([desc.get_editor_property("guid")
                                                   for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
                                                   if desc.get_editor_property("native_class") in wanted
                                                   or desc.get_editor_property("label") == "NightlightGround"])
actors = actor_subsystem.get_all_level_actors()
generator = next(actor for actor in actors if isinstance(actor, unreal.NightlightWorldGenerator))
scatter = next((actor for actor in actors if isinstance(actor, unreal.NightlightSceneryScatter)), None)
if scatter is None:
    scatter = actor_subsystem.spawn_actor_from_class(unreal.NightlightSceneryScatter, generator.get_actor_location())
    scatter.set_actor_label("NightlightSceneryScatter")

scatter.modify()
scatter.set_editor_property("world_generator", generator)
scatter.set_editor_property("rock_meshes", [unreal.load_asset(path) for path in ROCK_MESHES])
scatter.set_editor_property("border_meshes", [unreal.load_asset(path) for path in BORDER_MESHES])

# The pack's rocks and mountains share one master material that was never flagged for instanced
# meshes, so the game drew them with the default material. Set the flag once and save it.
master = unreal.load_asset(ROCK_MESHES[0]).get_material(0).get_base_material()
if not master.get_editor_property("used_with_instanced_static_meshes"):
    unreal.MaterialEditingLibrary.set_material_usage(master, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    unreal.EditorAssetLibrary.save_loaded_asset(master)

# The ground is the engine plane scaled to 60,000 units, centred on the grid and 20 units below
# the lowest a cell can reach, with collision off so it never catches clicks or traces.
settings = generator.get_editor_property("generation_settings")
half_size = (settings.get_editor_property("grid_width") - 1) * settings.get_editor_property("cell_size") * 0.5
origin = generator.get_actor_location()
ground_location = unreal.Vector(origin.x + half_size, origin.y + half_size,
                                origin.z - settings.get_editor_property("height_scale") - 20.0)
ground = next((actor for actor in actors if actor.get_actor_label() == "NightlightGround"), None)
if ground is None:
    ground = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, ground_location)
    ground.set_actor_label("NightlightGround")
ground.modify()
ground.set_actor_location(ground_location, False, False)
ground.set_actor_scale3d(unreal.Vector(600.0, 600.0, 1.0))
ground.set_editor_property("is_spatially_loaded", False)
mesh_component = ground.static_mesh_component
mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
mesh_component.set_material(0, unreal.load_asset("/Game/Desert_Castle/Desert_Castle_Vol1/Materials/Material_Instance/MI_Studio_Floor"))
mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

# Both actors are external, so only their own packages are saved, not the level.
unreal.EditorLoadingAndSavingUtils.save_packages([scatter.get_package(), ground.get_package()], True)
unreal.log("Nightlight scenery scatter placed as %s and pointed at %s." % (scatter.get_actor_label(), generator.get_actor_label()))

# References
#
# Epic Games, Inc., 2026a. unreal.EditorActorSubsystem. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EditorActorSubsystem>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026b. unreal.WorldPartitionBlueprintLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/WorldPartitionBlueprintLibrary>
# [Accessed 2 October 2026].
