# NightLight crayon HUD asset pack

Everything needed to build the "Crayons vs Nightmares" HUD from the preview (`Docs/UI/NightLight-HUD-Preview-Crayon.pdf` in the repo).

- **74 PNG textures**, all with transparent backgrounds (except the two mask textures, which are greyscale by design)
- **3 fonts** with their Open Font License files
- **Contact sheet**: `ContactSheet.png` shows every texture on a checkerboard so you can see the transparency

All art is drawn at 2× size (4× for the small tiles) so it stays sharp at 1080p and 1440p. The full-screen ink layers are 1920×1080.

## Folder layout

```
NightLight_Crayon_HUD/
  Textures/
    Icons/Defenders/    defender drawings, 3 line boil frames each
    Icons/Characters/   kid sun faces, nightmare beast, enemy marker
    Icons/HUD/          star token, slot figures, tier stars, lamp button
    Panels/             paper scraps, cards, notebook page, sticky note, tape, buttons
    SanityBar/          the tug of war bar pieces and hatch tiles
    Overlays/           range rings, pick circle, scribble-out, markers, rainbow
    Ink/                nightmare ink: vignettes, eyes, scribbles, splats, masks
  Fonts/
  (the HTML preview and PDF are in Docs/UI)
  ContactSheet.png / ContactSheet.html
```

## Line boil frames

Files ending `_F1`, `_F2`, `_F3` are the same drawing with slightly different wobble. Cycle them at about 8 frames per second (faster when sanity is low) to get the hand drawn "boiling line" look. If you don't want animation, just use `_F1`.

## 1. Defender drawings (`Icons/Defenders/`)

| File | Size (px) | What it is | Used in |
|---|---|---|---|
| `T_Defender_Patroller_F1..F3` | 512×410 | Blue paper plane with orange speed lines | Shop card, upgrade page portrait |
| `T_Defender_Shooter_F1..F3` | 512×410 | Wooden slingshot with a gold stone | Shop card, upgrade page portrait |
| `T_Defender_Torch_F1..F3` | 512×410 | Purple torch with yellow light rings (the Pulse defender) | Shop card, upgrade page portrait |
| `T_Defender_Core_F1..F3` | 512×410 | Pink crayon heart (the Dream Core) | Core shop card, Core upgrade page |

12 files.

## 2. Characters (`Icons/Characters/`)

| File | Size (px) | What it is | Used in |
|---|---|---|---|
| `T_Kid_Sun_Happy_F1..F3` | 384×384 | Smiling crayon sun, the kid's face | Left end of the sanity bar when sanity is 60% or more |
| `T_Kid_Sun_Scared_F1..F3` | 384×384 | Worried, sweating sun with faded colours | Left end of the sanity bar below 60% |
| `T_Nightmare_Beast_F1..F3` | 384×384 | Black ink monster with red eyes and teeth | Right end of the sanity bar |
| `T_Enemy_Marker_F1..F3` | 256×256 | Small ink ghost with red eyes | Optional marker over enemies or off-screen enemy arrows |

12 files.

## 3. HUD icons (`Icons/HUD/`)

| File | Size (px) | What it is | Used in |
|---|---|---|---|
| `T_Icon_StarToken` | 256×256 | Gold star sticker | Dream Token count, prices, "+10" pops |
| `T_Icon_SlotFigure_On` | 128×192 | Stick figure, full colour | One per used defender slot |
| `T_Icon_SlotFigure_Off` | 128×192 | Same figure, faded | One per free defender slot |
| `T_Icon_Star_Empty` | 256×256 | Grey outline star | Upgrade tiers not reached yet |
| `T_Icon_Star_Next` | 256×256 | Dashed pencil star | The next upgrade tier (bought tiers use `T_Icon_StarToken`) |
| `T_Button_Lamp_F1..F3` | 560×600 | Bedside lamp with a pull chain | "LIGHTS OUT" start wave button |

8 files.

## 4. Paper and buttons (`Panels/`)

These are meant to be drawn as **Box (9-slice)** brushes in UMG so they can stretch to any size. Suggested margins are given as a fraction of the texture, which is what UMG's Margin field expects.

| File | Size (px) | What it is | Margin | Used in |
|---|---|---|---|---|
| `T_Paper_Scrap` | 800×400 | Torn cream paper scrap | 0.1 | Token count, slot count, wave summary |
| `T_Paper_Card` | 472×600 | Tall paper card | 0.12 | Each shop card |
| `T_Notebook_Page` | 840×1160 | Lined notebook page with red margin and punched holes | none (draw as Image) | Upgrade panel for defenders and the Core |
| `T_StickyNote_Yellow` | 600×440 | Yellow sticky note | 0.1 | "FASTER!!" speed toggle during waves |
| `T_Tape_01..03` | 280×100 | Masking tape strips, three tear shapes | none | Stuck on the corners of scraps and cards, rotated a few degrees |
| `T_Button_Crayon_Green` | 640×160 | Green crayon button | 0.2 | "MAKE IT STRONGER" upgrade button, normal |
| `T_Button_Crayon_Green_Hover` | 640×160 | Lighter green | 0.2 | Same button, hovered |
| `T_Button_Crayon_Disabled` | 640×160 | Grey crayon | 0.2 | Same button when tokens are short |
| `T_Button_Crayon_Blue` | 640×160 | Blue crayon | 0.2 | Secondary buttons (menus, "let go", pause menu) |

11 files.

## 5. Sanity bar (`SanityBar/`)

Stack these in one widget, from back to front: Paper, InkBack, the yellow fill, EdgeRed, Frame.

| File | Size (px) | What it is | How to use it |
|---|---|---|---|
| `T_SanityBar_Paper` | 1160×172 | Paper strip behind the bar | Bottom layer |
| `T_SanityBar_InkBack` | 1160×172 | Wet ink strip | Progress Bar **background** image (the nightmare's side) |
| `T_Hatch_Yellow_Tile` | 256×256 | Tileable yellow crayon hatching | Progress Bar **fill** image, set to Tile. Sanity = Core health / max health. |
| `T_Hatch_Grey_Tile` | 256×256 | Tileable drained grey hatching | Swap in for the fill below 30% sanity |
| `T_SanityBar_EdgeRed` | 96×172 | Jagged red line | Moved along the bar to sit where the fill ends |
| `T_SanityBar_Frame` | 1160×172 | Wobbly pencil outline | Top layer |

6 files.

## 6. Map overlays (`Overlays/`)

| File | Size (px) | What it is | Used in |
|---|---|---|---|
| `T_Range_Ring_Yellow` | 1024×1024 | Wobbly crayon circle | Range preview while placing a defender |
| `T_Range_Ring_Blue` | 1024×1024 | Same in blue | Range of the selected defender in the upgrade view |
| `T_Range_Ring_Red` | 1024×1024 | Same in red | Range preview where placing is not allowed |
| `T_Pick_Circle_Red` | 320×380 | Hand drawn red circle | Drawn around the chosen shop card |
| `T_ScribbleOut_Grey` | 472×600 | Grey pencil scribble | Laid over a shop card the player can't afford |
| `T_Placement_X` | 192×192 | Yellow crayon X | Marks free platforms during the build phase (add the "here!" text in UMG) |
| `T_Arrow_Crayon` | 512×256 | Yellow crayon arrow | Points from the upgrade page to the selected defender, or tutorial hints |
| `T_Rainbow_WeDidIt` | 1280×360 | Four colour crayon rainbow | Behind the "WE DID IT!!" text when a wave is cleared |

The range rings can be used as world space decals or as screen space widgets, whichever is easier with the camera.

8 files.

## 7. Nightmare ink (`Ink/`)

| File | Size (px) | What it is | Used in |
|---|---|---|---|
| `T_Ink_Vignette_Brave` | 1920×1080 | Ink only in the corners | Full-screen ink, sanity 100 to 60% |
| `T_Ink_Vignette_Scared` | 1920×1080 | Ink creeping in from the edges | Sanity 60 to 30% |
| `T_Ink_Vignette_Losing` | 1920×1080 | Thick ink around the whole edge | Sanity 30 to 15% |
| `T_Ink_Vignette_Overrun` | 1920×1080 | Ink covering most of the edges | Sanity below 15% |
| `T_Nightmare_Eyes` | 256×128 | Pair of red eyes | Faded in inside the ink below 30% sanity (use 2 or 3 copies) |
| `T_Ink_Scribble_01..06` | 512×400 | Frantic black scribbles | Scattered over panels and the screen edge as sanity drops |
| `T_Ink_Splat_01..04` | 512×512 | Ink splats with spatter | Where nightmares hit the Core, or on panel corners |
| `T_Ink_NoiseMask_Tile` | 512×512 | Tileable greyscale cloud noise | Mask for the ink material (see below). Import with sRGB off. |
| `T_Ink_EdgeGradient` | 1024×576 | Black centre, white edges | Mask for the ink material. Import with sRGB off. |

17 files.

### Two ways to do the creeping ink

1. **Simple:** stack the four vignette images and cross-fade between them as sanity changes.
2. **Smooth:** one full-screen Image with a UI material. Multiply `T_Ink_EdgeGradient` by `T_Ink_NoiseMask_Tile` (pan the noise slowly), compare the result against a `Sanity` scalar parameter, and use that as the opacity mask over solid ink colour `#0A0509`. Lower sanity lets more ink through, and it grows inward from the edges.

## 8. Fonts (`Fonts/`)

| File | Used for |
|---|---|
| `GochiHand-Regular.ttf` | The kid's handwriting: every label, number, button and panel he "wrote" |
| `RubikWetPaint-Regular.ttf` | The nightmare's dripping red text: wave label ("NIGHT 7 · 23 LEFT") and warnings |
| `RubikGlitch-Regular.ttf` | Replaces Gochi Hand on corrupted labels when sanity is below 30% |

All three are under the SIL Open Font License 1.1 (licence files included), so they can be used and shipped in the game for free.

## 9. Colours

| Name | Hex | Use |
|---|---|---|
| Notebook paper | `#F4ECD6` | Paper scraps and text backgrounds |
| Pencil | `#3A3A46` | The kid's text |
| Sun crayon | `#FFC93C` | Sanity fill, stars, highlights |
| Orange crayon | `#FF8A3D` | Accents |
| Sky crayon | `#4AA3FF` | Titles on the notebook page, selected range |
| Grass crayon | `#5CC96B` | Buy / upgrade button |
| Nightmare ink | `#0A0509` | All ink |
| Nightmare red | `#D21F3C` | Nightmare text, eyes, warnings, stat increases |

## 10. Import settings (for when you bring them into Unreal)

- **Texture Group:** UI. **Compression:** UserInterface2D (RGBA). **Mip Gen:** NoMipmaps. **Never Stream:** on.
- The two mask textures (`T_Ink_NoiseMask_Tile`, `T_Ink_EdgeGradient`): sRGB **off**, Compression **Grayscale** or **Masks**.
- Tile textures (`T_Hatch_*`): Address X and Y set to **Wrap**.
- Fonts: import each `.ttf` as a Font Face, then make one Font asset per typeface.

## Swapping in real crayon art later

Every file name and size here matches what the HUD would use, so if you draw the pieces with real crayons and scan them, you can save them over these files with the same name and size and nothing else needs to change.

## Totals

| Category | Files |
|---|---|
| Defender drawings | 12 |
| Characters | 12 |
| HUD icons | 8 |
| Paper and buttons | 11 |
| Sanity bar | 6 |
| Map overlays | 8 |
| Nightmare ink | 17 |
| **Textures total** | **74** |
| Fonts | 3 (+3 licence files) |
