"""Builds M_NightlightTerrain and MI_NightlightTerrain for the generated terrain.

The world generator writes one mask per vertex colour channel: R path, G anchor,
B Rift and A Core. This graph sharpens those masks, breaks their edges up with
world-space noise and blends grass, high ground, slope rock, a dirt track, stone
build spots and glowing Rift and Core areas from them. Every value is a parameter,
so the look is tuned on MI_NightlightTerrain without opening the graph.

Run with the editor closed:
UnrealEditor-Cmd.exe "<absolute path>/NightLightV2.uproject" -run=pythonscript "-script=__import__('nightlight_terrain_material')"
The editor puts Content/Python on the Python path, so importing the module runs it. This avoids
the commandlet splitting -script at the spaces in the project path.
Running it again rebuilds the graph and keeps any values already set on the instance.
"""
import unreal

# Material graphs are built and saved through the editor scripting library
# (Epic Games, Inc., 2026a; Epic Games, Inc., 2026d).
MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
FOLDER = "/Game/Materials/Terrain"
MATERIAL_PATH = FOLDER + "/M_NightlightTerrain"
INSTANCE_PATH = FOLDER + "/MI_NightlightTerrain"
TEXTURES = "/Game/Desert_Castle/Desert_Castle_Vol1/Textures/"

# Nodes are laid out left to right in 600-unit columns: inputs, masks, surfaces, blends, outputs.
COLUMN = 600
ROW = 140


def load_or_create(path, asset_class, factory):
    """Loads the asset if an earlier run made it, otherwise creates it."""
    if ASSETS.does_asset_exist(path):
        return ASSETS.load_asset(path)
    name = path.rsplit("/", 1)[1]
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, asset_class, factory)


material = load_or_create(MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
for old_expression in MEL.get_material_expressions(material):
    MEL.delete_material_expression(material, old_expression)


def node(cls, column, row, **properties):
    expression = MEL.create_material_expression(material, cls, int(-3600 + column * COLUMN), row * ROW)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def link(source, target, target_input="", source_output=""):
    if not MEL.connect_material_expressions(source, source_output, target, target_input):
        raise RuntimeError("Could not connect %s to %s.%s" % (source.get_name(), target.get_name(), target_input))


def scalar(name, value, group, column, row):
    return node(unreal.MaterialExpressionScalarParameter, column, row, parameter_name=name, default_value=value, group=group)


def colour(name, rgb, group, column, row):
    return node(unreal.MaterialExpressionVectorParameter, column, row, parameter_name=name,
                default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0), group=group)


def maths(cls, a, b, column, row, a_output="", b_output=""):
    """Adds a two-input maths node (Add, Subtract, Multiply, Divide) fed by a and b."""
    expression = node(cls, column, row)
    link(a, expression, "A", a_output)
    link(b, expression, "B", b_output)
    return expression


def lerp(a, b, alpha, column, row, alpha_output=""):
    expression = node(unreal.MaterialExpressionLinearInterpolate, column, row)
    link(a, expression, "A")
    link(b, expression, "B")
    link(alpha, expression, "Alpha", alpha_output)
    return expression


def noise(position, scale, column, row):
    """World-space noise from 0 to 1, so the pattern does not stretch with the grid UVs.
    Simplex noise can overshoot its output range, so the result is saturated."""
    scaled = maths(unreal.MaterialExpressionMultiply, position, scale, column, row, a_output="XYZ")
    expression = node(unreal.MaterialExpressionNoise, column, row + 1, output_min=0.0, output_max=1.0,
                      levels=2, turbulence=False)
    link(scaled, expression, "World Position")
    clamped = node(unreal.MaterialExpressionSaturate, column + 0.4, row + 1)
    link(expression, clamped)
    return clamped


# Inputs.
world_position = node(unreal.MaterialExpressionWorldPosition, 0, 0)
vertex_colour = node(unreal.MaterialExpressionVertexColor, 0, 3)
world_normal = node(unreal.MaterialExpressionVertexNormalWS, 0, 6)
grass_noise_scale = scalar("GrassNoiseScale", 0.0015, "Grass", 0, 8)
edge_noise_scale = scalar("EdgeNoiseScale", 0.01, "Masks", 0, 9)
edge_noise_strength = scalar("EdgeNoiseStrength", 0.25, "Masks", 0, 10)
mask_edge_min = scalar("MaskEdgeMin", 0.4, "Masks", 0, 11)
mask_edge_max = scalar("MaskEdgeMax", 0.6, "Masks", 0, 12)
min_height = scalar("TerrainMinHeight", -300.0, "Height", 0, 14)
max_height = scalar("TerrainMaxHeight", 300.0, "Height", 0, 15)
slope_start = scalar("SlopeRockStart", 0.85, "Rock", 0, 17)
slope_blend = scalar("SlopeRockBlend", 0.08, "Rock", 0, 18)

# Masks. One cell per vertex makes each mask fade over a whole cell, so the
# SmoothStep turns it into a short edge and the noise makes that edge uneven.
grass_noise = noise(world_position, grass_noise_scale, 1, 0)
edge_noise_centred = node(unreal.MaterialExpressionSubtract, 1.4, 5, const_b=0.5)
link(noise(world_position, edge_noise_scale, 1, 3), edge_noise_centred, "A")
edge_noise = maths(unreal.MaterialExpressionMultiply, edge_noise_centred, edge_noise_strength, 1, 5)


def sharpen(source, row, source_output=""):
    noisy = maths(unreal.MaterialExpressionAdd, source, edge_noise, 1, row, a_output=source_output)
    expression = node(unreal.MaterialExpressionSmoothStep, 1, row + 1)
    link(mask_edge_min, expression, "Min")
    link(mask_edge_max, expression, "Max")
    link(noisy, expression, "Value")
    return expression


def node_scaled(source, factor, column, row):
    expression = node(unreal.MaterialExpressionMultiply, column, row, const_b=factor)
    link(source, expression, "A")
    return expression

def ring(source, row, source_output=""):
    """Lights a narrow band where a mask's fade crosses 0.5, which draws a ring around the cell.
    The band is 1 - |mask - 0.5| * 5, so it is zero below 0.3 and stays dark on open ground."""
    noisy = maths(unreal.MaterialExpressionAdd, source, edge_noise, 1, row, a_output=source_output)
    centred = node(unreal.MaterialExpressionSubtract, 1, row + 1, const_b=0.5)
    link(noisy, centred, "A")
    distance = node(unreal.MaterialExpressionAbs, 1, row + 2)
    link(centred, distance)
    inverse = node(unreal.MaterialExpressionOneMinus, 1, row + 4)
    link(node_scaled(distance, 5.0, 1, row + 3), inverse)
    band = node(unreal.MaterialExpressionSaturate, 1.4, row + 4)
    link(inverse, band)
    return band



# Rifts and the Core sit at the ends of the paths, so they share the dirt track.
route_ends = maths(unreal.MaterialExpressionAdd, vertex_colour, vertex_colour, 1, 7, "B", "A")
track = maths(unreal.MaterialExpressionAdd, vertex_colour, route_ends, 1, 8, a_output="R")
path_mask = sharpen(track, 9)
anchor_mask = sharpen(vertex_colour, 11, "G")
rift_mask = sharpen(vertex_colour, 13, "B")
anchor_rim = ring(vertex_colour, 15, "G")
core_ring = ring(vertex_colour, 20, "A")

# Height: 0 at the lowest cell and 1 at the highest, for this seed.
above_min = maths(unreal.MaterialExpressionSubtract, world_position, min_height, 1, 26, a_output="Z")
height_range = maths(unreal.MaterialExpressionSubtract, max_height, min_height, 1, 27)
height_ratio = maths(unreal.MaterialExpressionDivide, above_min, height_range, 1, 28)
height_alpha = node(unreal.MaterialExpressionSaturate, 1, 29)
link(height_ratio, height_alpha)
high_ground_strength = scalar("HighGroundStrength", 0.7, "Height", 1, 30)
height_blend = maths(unreal.MaterialExpressionMultiply, height_alpha, high_ground_strength, 1, 31)

# Slope: rock where the surface normal points less straight up than SlopeRockStart.
normal_z = node(unreal.MaterialExpressionComponentMask, 1, 33, r=False, g=False, b=True, a=False)
link(world_normal, normal_z)
slope_low = maths(unreal.MaterialExpressionSubtract, slope_start, slope_blend, 1, 34)
flatness = node(unreal.MaterialExpressionSmoothStep, 1, 35)
link(slope_low, flatness, "Min")
link(slope_start, flatness, "Max")
link(normal_z, flatness, "Value")
slope_alpha = node(unreal.MaterialExpressionOneMinus, 1, 36)
link(flatness, slope_alpha)

# Surfaces.
grass = lerp(colour("GrassColourA", (0.05, 0.16, 0.04), "Grass", 2, 0),
             colour("GrassColourB", (0.12, 0.28, 0.06), "Grass", 2, 2), grass_noise, 2, 4)
high_ground = colour("HighGroundColour", (0.32, 0.36, 0.16), "Height", 2, 5)

# World-aligned projection keeps the rock from stretching on steep faces
# (Epic Games, Inc., 2026b).
rock_size = scalar("RockTextureSize", 400.0, "Rock", 2, 7)
rock_albedo = node(unreal.MaterialExpressionTextureObjectParameter, 2, 8, parameter_name="RockAlbedo", group="Rock",
                   texture=unreal.load_asset(TEXTURES + "Desert_Rock_02/T_Desert_Rock_02_Albedo"))
rock_normal_texture = node(unreal.MaterialExpressionTextureObjectParameter, 2, 10, parameter_name="RockNormal",
                           group="Rock", sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                           texture=unreal.load_asset(TEXTURES + "Desert_Rock_02/T_Desert_Rock_02_Normal"))
rock_projection = node(unreal.MaterialExpressionMaterialFunctionCall, 2, 12)
rock_projection.set_material_function(
    unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"))
link(rock_albedo, rock_projection, "TextureObject")
link(rock_size, rock_projection, "TextureSize")
rock_normal = node(unreal.MaterialExpressionMaterialFunctionCall, 2, 14)
rock_normal.set_material_function(
    unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal"))
link(rock_normal_texture, rock_normal, "TextureObject")
link(rock_size, rock_normal, "TextureSize")
rock = maths(unreal.MaterialExpressionMultiply, rock_projection, colour("RockTint", (0.6, 0.55, 0.5), "Rock", 2, 16),
             2, 18, a_output="XYZ Texture")

# The track is darker where the raw path mask is strongest, so the middle looks worn.
path_uv = maths(unreal.MaterialExpressionMultiply, world_position,
                scalar("PathTextureScale", 0.002, "Path", 2, 19), 2, 20, a_output="XY")
sand = node(unreal.MaterialExpressionTextureSampleParameter2D, 2, 21, parameter_name="PathAlbedo", group="Path",
            texture=unreal.load_asset(TEXTURES + "Desert_Dry_Sand/T_Dry_Sand_Albedo"))
link(path_uv, sand, "UVs")
sand_normal = node(unreal.MaterialExpressionTextureSampleParameter2D, 2, 23, parameter_name="PathNormal", group="Path",
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                   texture=unreal.load_asset(TEXTURES + "Desert_Dry_Sand/T_Dry_Sand_Normal"))
link(path_uv, sand_normal, "UVs")
wear = node(unreal.MaterialExpressionLinearInterpolate, 2, 25, const_a=1.0)
link(scalar("PathCentreShade", 0.75, "Path", 2, 26), wear, "B")
link(vertex_colour, wear, "Alpha", "R")
path = maths(unreal.MaterialExpressionMultiply, sand, wear, 2, 27, a_output="RGB")

# Build spots reuse the rock without its colour, tinted to a pale cut stone.
stone = node(unreal.MaterialExpressionDesaturation, 2, 29)
link(rock_projection, stone, "", "XYZ Texture")
anchor = maths(unreal.MaterialExpressionMultiply, stone,
               colour("AnchorStoneColour", (0.85, 0.85, 0.8), "Anchor", 2, 30), 2, 32)

# Base colour, layered from the ground up.
base = lerp(grass, high_ground, height_blend, 3, 0)
base = lerp(base, rock, slope_alpha, 3, 2)
base = lerp(base, path, path_mask, 3, 4)
base = lerp(base, anchor, anchor_mask, 3, 6)
MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

# Normal: flat grass, world-aligned rock on slopes, sand on the track.
flat_normal = node(unreal.MaterialExpressionConstant3Vector, 3, 9, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
rock_normal_blend = node(unreal.MaterialExpressionLinearInterpolate, 3, 10)
link(flat_normal, rock_normal_blend, "A")
link(rock_normal, rock_normal_blend, "B", "XYZ Texture")
link(slope_alpha, rock_normal_blend, "Alpha")
surface_normal = lerp(rock_normal_blend, sand_normal, path_mask, 3, 12)
MEL.connect_material_property(surface_normal, "", unreal.MaterialProperty.MP_NORMAL)

# Roughness follows the same layers.
roughness = lerp(scalar("GrassRoughness", 0.9, "Grass", 3, 14), scalar("RockRoughness", 0.8, "Rock", 3, 15),
                 slope_alpha, 3, 16)
roughness = lerp(roughness, scalar("PathRoughness", 0.95, "Path", 3, 17), path_mask, 3, 18)
roughness = lerp(roughness, scalar("AnchorRoughness", 0.7, "Anchor", 3, 19), anchor_mask, 3, 20)
MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)


def glow(colour_name, rgb, strength_name, strength, mask, group, row):
    tint = maths(unreal.MaterialExpressionMultiply, colour(colour_name, rgb, group, 4, row),
                 scalar(strength_name, strength, group, 4, row + 2), 4, row + 3)
    return maths(unreal.MaterialExpressionMultiply, tint, mask, 4, row + 4)


# Emissive: purple Rifts, a gold ring around the Core and a faint rim on free build spots.
rift_glow = glow("RiftColour", (0.6, 0.1, 1.0), "RiftGlow", 8.0, rift_mask, "Rift and Core", 0)
core_glow = glow("CoreRingColour", (1.0, 0.75, 0.2), "CoreGlow", 6.0, core_ring, "Rift and Core", 6)
anchor_glow = glow("AnchorRimColour", (0.4, 0.8, 1.0), "AnchorRimGlow", 2.0, anchor_rim, "Anchor", 12)
emissive = maths(unreal.MaterialExpressionAdd, rift_glow, core_glow, 5, 4)
emissive = maths(unreal.MaterialExpressionAdd, emissive, anchor_glow, 5, 6)
MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(material)
ASSETS.save_loaded_asset(material)

# The instance keeps its own overrides, so a second run never resets tuned values.
instance = load_or_create(INSTANCE_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
MEL.set_material_instance_parent(instance, material)
MEL.update_material_instance(instance)
ASSETS.save_loaded_asset(instance)
unreal.log("Nightlight terrain material built with %d nodes." % MEL.get_num_material_expressions(material))

# Give the placed generator the instance. World Partition only loads actors near the
# editor camera, so the generator is loaded by class, and as an external actor only its
# own package is saved (Epic Games, Inc., 2026c; Epic Games, Inc., 2026e).
unreal.EditorLoadingAndSavingUtils.load_map("/Game/NightLightV2")
generator_class = unreal.NightlightWorldGenerator.static_class()
unreal.WorldPartitionBlueprintLibrary.load_actors([desc.get_editor_property("guid")
                                                   for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
                                                   if desc.get_editor_property("native_class") == generator_class])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for generator in [actor for actor in actors if isinstance(actor, unreal.NightlightWorldGenerator)]:
    # The level's flat grid landscape reaches Z 100 and hid the low ground, so the generator is lifted
    # until even its lowest possible cell (-HeightScale) stays 50 units above it.
    location = generator.get_actor_location()
    lifted_z = 150.0 + generator.get_editor_property("generation_settings").get_editor_property("height_scale")
    if generator.get_editor_property("terrain_material") != instance or location.z != lifted_z:
        generator.modify()
        generator.set_editor_property("terrain_material", instance)
        generator.set_actor_location(unreal.Vector(location.x, location.y, lifted_z), False, False)
        unreal.EditorLoadingAndSavingUtils.save_packages([generator.get_package()], True)
    unreal.log("Nightlight terrain material set on %s." % generator.get_actor_label())

# References
#
# Epic Games, Inc., 2026a. Scripting the Unreal Editor Using Python. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026b. Texturing Material Functions in Unreal Engine. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/texturing-material-functions-in-unreal-engine>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026c. unreal.EditorActorSubsystem. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EditorActorSubsystem>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026d. unreal.MaterialEditingLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialEditingLibrary>
# [Accessed 2 October 2026].
#
# Epic Games, Inc., 2026e. unreal.WorldPartitionBlueprintLibrary. [online] Available at:
# <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/WorldPartitionBlueprintLibrary>
# [Accessed 2 October 2026].
