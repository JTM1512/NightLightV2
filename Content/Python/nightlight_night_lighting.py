"""Lights /Game/NightLightV2 as a calm moonlit night and tunes the glow for readability.

Turns the template sun, sky, sky light and fog into a low blue moon, a deep blue sky and a
thin blue-purple haze, adds an unbound post process volume with a fixed exposure, light bloom
and a cool white balance, and gives BP_DreamCore a warm point light so the Core is the brightest
place on the map. It then sets the glow on MI_NightlightTerrain and the enemy colours so paths,
build spots and each enemy type still stand out in the dark.

Run with the editor closed, after the terrain material and scenery scripts:
UnrealEditor-Cmd.exe "<absolute path>/NightLightV2.uproject" -run=pythonscript "-script=__import__('nightlight_night_lighting')"
Each actor is found by label, or spawned once, and only what differs is changed and saved,
so running it again changes nothing.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def same(current, wanted):
    """Floats are compared loosely because the engine stores them at single precision, and
    structs such as colours by their text, because two equal structs do not compare equal."""
    if isinstance(wanted, float):
        return abs(current - wanted) < 1e-4
    if isinstance(wanted, unreal.StructBase):
        return current.export_text() == wanted.export_text()
    return current == wanted


def update(target, values):
    """Sets each property that differs from the wanted value and returns whether any did."""
    changed = [name for name, value in values.items() if not same(target.get_editor_property(name), value)]
    for name in changed:
        target.set_editor_property(name, values[name])
    return bool(changed)


# Level actors are found and spawned through the editor actor subsystem (Epic Games, Inc., 2026k).
# The template sky and lights are not spatially loaded, so they load with the map.
unreal.EditorLoadingAndSavingUtils.load_map("/Game/NightLightV2")
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {actor.get_actor_label(): actor for actor in actor_subsystem.get_all_level_actors()}
changed_actors = []


def light_actor(label, actor_class, component_class, values, rotation=None):
    actor = actors.get(label)
    if actor is None:
        actor = actor_subsystem.spawn_actor_from_class(actor_class, unreal.Vector(0.0, 0.0, 0.0))
        actor.set_actor_label(label)
    actor.modify()
    component = actor.get_component_by_class(component_class)
    changed = update(component, values)
    if rotation is not None:
        changed = update(component, {"relative_rotation": rotation}) or changed
    if changed:
        changed_actors.append(actor)
    return actor


# Moonlight: a low, dim, cool directional light. A wider source angle softens the shadow
# edges, and it still drives the sky atmosphere (Epic Games, Inc., 2026d).
light_actor("DirectionalLight", unreal.DirectionalLight, unreal.DirectionalLightComponent, {
    "intensity": 1.5, "light_color": unreal.Color(r=170, g=190, b=255, a=255),
    "light_source_angle": 4.0, "atmosphere_sun_light": True,
}, unreal.Rotator(roll=0.0, pitch=-28.0, yaw=135.0))

# Sky: the atmosphere's luminance is pushed towards blue, so the dim, low moon gives a deep
# blue night instead of a grey sunset (Epic Games, Inc., 2026h).
light_actor("SkyAtmosphere", unreal.SkyAtmosphere, unreal.SkyAtmosphereComponent, {
    "sky_luminance_factor": unreal.LinearColor(0.6, 0.9, 2.0, 1.0),
})

# The sky light captures the night sky in real time, and a dark blue lower hemisphere keeps
# shadowed ground blue rather than black (Epic Games, Inc., 2026i).
light_actor("SkyLight", unreal.SkyLight, unreal.SkyLightComponent, {
    "real_time_capture": True, "intensity": 2.0, "light_color": unreal.Color(r=150, g=175, b=255, a=255),
    "lower_hemisphere_is_black": False, "lower_hemisphere_color": unreal.LinearColor(0.01, 0.015, 0.04, 1.0),
})

# Fog: thin and blue-purple, so the border mountains fade out and the glow stands out (Epic Games, Inc., 2026e).
light_actor("ExponentialHeightFog", unreal.ExponentialHeightFog, unreal.ExponentialHeightFogComponent, {
    "fog_density": 0.01, "fog_height_falloff": 0.2,
    "fog_inscattering_luminance": unreal.LinearColor(0.012, 0.012, 0.035, 1.0),
})

# Post process: an unbound volume whose exposure range is one value, so the exposure never
# adapts as the camera moves, with light bloom for the emissives and a cooler white balance
# (Epic Games, Inc., 2026a; Epic Games, Inc., 2026b; Epic Games, Inc., 2026c; Epic Games, Inc., 2026g).
EXPOSURE_EV100 = 0.5
post_process = actors.get("NightlightPostProcess")
if post_process is None:
    post_process = actor_subsystem.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0))
    post_process.set_actor_label("NightlightPostProcess")
post_process.modify()
settings = post_process.get_editor_property("settings")
settings_changed = update(settings, {
    "override_auto_exposure_method": True, "auto_exposure_method": unreal.AutoExposureMethod.AEM_HISTOGRAM,
    "override_auto_exposure_min_brightness": True, "auto_exposure_min_brightness": EXPOSURE_EV100,
    "override_auto_exposure_max_brightness": True, "auto_exposure_max_brightness": EXPOSURE_EV100,
    "override_auto_exposure_bias": True, "auto_exposure_bias": 0.0,
    "override_bloom_intensity": True, "bloom_intensity": 0.8,
    "override_white_temp": True, "white_temp": 5600.0,
})
if settings_changed:
    post_process.set_editor_property("settings", settings)
if update(post_process, {"unbound": True}) or settings_changed:
    changed_actors.append(post_process)

if changed_actors:
    unreal.EditorLoadingAndSavingUtils.save_packages([actor.get_package() for actor in changed_actors], True)
unreal.log("Nightlight night lighting changed %d actors." % len(changed_actors))

# Core light: a warm point light in BP_DreamCore, added through the subobject data subsystem and
# compiled (Epic Games, Inc., 2026f; Epic Games, Inc., 2026j; Epic Games, Inc., 2026n). It sits above
# the tower's middle and casts no shadow, so the tower walls do not block it and it costs little.
core = unreal.load_asset("/Game/Blueprints/BP_DreamCore")
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles = subobjects.k2_gather_subobject_data_for_blueprint(core)
components = [unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
              for handle in handles]
core_light = next((component for component in components if isinstance(component, unreal.PointLightComponent)), None)
if core_light is None:
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.PointLightComponent, blueprint_context=core)
    handle, _ = subobjects.add_new_subobject(params)
    subobjects.rename_subobject(handle, "CoreLight")
    core_light = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
# A point light's intensity is in candelas here, so the brightness does not depend on its radius
# (Epic Games, Inc., 2026o).
if update(core_light, {
    "relative_location": unreal.Vector(390.0, -350.0, 900.0), "intensity_units": unreal.LightUnits.CANDELAS,
    "intensity": 1500.0, "light_color": unreal.Color(r=255, g=185, b=110, a=255),
    "attenuation_radius": 3000.0, "cast_shadows": False,
}):
    unreal.BlueprintEditorLibrary.compile_blueprint(core)
    ASSETS.save_loaded_asset(core)
    unreal.log("Nightlight Core light set on BP_DreamCore.")

# Glow and colour tuning on material instance parameters only (Epic Games, Inc., 2026l; Epic Games, Inc., 2026m).
# The enemy master material has no glow parameter, so each enemy type is told apart by a brighter,
# more distinct base colour: a pale green Walker, a red Brute and a magenta Shade, which is unlit
# and so glows with its own colour.
INSTANCES = {
    "/Game/Materials/Terrain/MI_NightlightTerrain": {"RiftGlow": 12.0, "CoreGlow": 6.0, "AnchorRimGlow": 4.0,
                                                     "PathCentreShade": 0.9},
    "/Game/Materials/Enemies/MI_Enemy_Walker": {"BaseColour": unreal.LinearColor(0.3, 0.45, 0.32, 1.0)},
    "/Game/Materials/Enemies/MI_Enemy_Brute": {"BaseColour": unreal.LinearColor(0.6, 0.03, 0.02, 1.0)},
    "/Game/Materials/Enemies/MI_Enemy_Shade": {"BaseColour": unreal.LinearColor(0.35, 0.03, 0.3, 1.0)},
}
for path, values in INSTANCES.items():
    instance = unreal.load_asset(path)
    changed = False
    for name, value in values.items():
        scalar = isinstance(value, float)
        current = (MEL.get_material_instance_scalar_parameter_value if scalar
                   else MEL.get_material_instance_vector_parameter_value)(instance, name)
        if not same(current, value):
            (MEL.set_material_instance_scalar_parameter_value if scalar
             else MEL.set_material_instance_vector_parameter_value)(instance, name, value)
            changed = True
    if changed:
        MEL.update_material_instance(instance)
        ASSETS.save_loaded_asset(instance)
        unreal.log("Nightlight glow tuned on %s." % path)

# References
#
# Epic Games, Inc., 2026a. Auto Exposure in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026b. Bloom in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/bloom-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026c. Color Grading and the Filmic Tonemapper in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/color-grading-and-the-filmic-tonemapper-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026d. Directional Lights in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/directional-lights-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026e. Exponential Height Fog in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/exponential-height-fog-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026f. Point Lights in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/point-lights-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026g. Post Process Effects in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-effects-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026h. Sky Atmosphere Component in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026i. Sky Lights in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026j. unreal.BlueprintEditorLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/BlueprintEditorLibrary>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026k. unreal.EditorActorSubsystem. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EditorActorSubsystem>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026l. unreal.EditorAssetLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EditorAssetLibrary>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026m. unreal.MaterialEditingLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialEditingLibrary>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026n. unreal.SubobjectDataSubsystem. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SubobjectDataSubsystem>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026o. Using Physical Lighting Units in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-physical-lighting-units-in-unreal-engine>
# [Accessed 2 October 2026].
