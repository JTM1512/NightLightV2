"""Gives the player pawn a camera height control and starts it above the terrain.

Adds a NightlightCameraHeightComponent to BP_TDPlayerPawn (E and Q or the mouse
wheel move the camera up and down) and raises the PlayerStart in /Game/NightLightV2
to the generator's height, because the generator was lifted above the flat grid
landscape and the old start left the camera under the hills.

Run with the editor closed:
UnrealEditor-Cmd.exe "<absolute path>/NightLightV2.uproject" -run=pythonscript "-script=__import__('nightlight_player_view')"
Running it again adds nothing new and saves nothing that is already set.
"""
import unreal

# Components are added to a Blueprint through the subobject data subsystem, then the
# Blueprint is compiled and saved (Epic Games, Inc., 2026a; Epic Games, Inc., 2026c).
pawn = unreal.load_asset("/Game/Blueprints/BP_TDPlayerPawn")
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles = subobjects.k2_gather_subobject_data_for_blueprint(pawn)
existing = [unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
            for handle in handles]
if not any(isinstance(component, unreal.NightlightCameraHeightComponent) for component in existing):
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.NightlightCameraHeightComponent,
                                          blueprint_context=pawn)
    subobjects.add_new_subobject(params)
    unreal.BlueprintEditorLibrary.compile_blueprint(pawn)
    unreal.EditorAssetLibrary.save_loaded_asset(pawn)
    unreal.log("Nightlight camera height component added to BP_TDPlayerPawn.")

# The PlayerStart is always loaded, so it is found among the level actors and only its
# own external package is saved (Epic Games, Inc., 2026b).
unreal.EditorLoadingAndSavingUtils.load_map("/Game/NightLightV2")
generator_class = unreal.NightlightWorldGenerator.static_class()
unreal.WorldPartitionBlueprintLibrary.load_actors([desc.get_editor_property("guid")
                                                   for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
                                                   if desc.get_editor_property("native_class") == generator_class])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
generator = next(actor for actor in actors if isinstance(actor, unreal.NightlightWorldGenerator))
for start in [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]:
    location = start.get_actor_location()
    if location.z < generator.get_actor_location().z:
        start.modify()
        start.set_actor_location(unreal.Vector(location.x, location.y, generator.get_actor_location().z), False, False)
        unreal.EditorLoadingAndSavingUtils.save_packages([start.get_package()], True)
    unreal.log("Nightlight player start is at Z %.0f." % start.get_actor_location().z)

# References
#
# Epic Games, Inc., 2026a. unreal.BlueprintEditorLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/BlueprintEditorLibrary>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026b. unreal.EditorActorSubsystem. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EditorActorSubsystem>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026c. unreal.SubobjectDataSubsystem. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SubobjectDataSubsystem>
# [Accessed 2 October 2026].
