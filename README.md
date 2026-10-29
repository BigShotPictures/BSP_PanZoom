# BSP 2D Pan/Zoom

Maya-style 2D pan/zoom for Unreal editor viewports. Zoom into and pan around what a shot camera sees **without moving the camera, changing its focal length, or touching a single Sequencer key**. Renders, Movie Render Queue and Play-In-Editor never see it.

Based on [Loops 2D Pan/Zoom](https://github.com/LoopsCreativeStudio/Loops2dPanZoom) 1.8.0 by Loops Creative Studio (MIT, see `LICENSE`), with the lens-shift pan, Python API and toolbar dropdown from Big Shot Pictures' own BSP_PanZoom design. This is a permanent BSP fork; nothing is merged back upstream.

## Installation

Ships prebuilt in `//Plugins/5.8/BSP/BSP_PanZoom` (source + `Binaries/Win64`), like `BLFClayPass`. Projects pick it up through `AdditionalPluginDirectories` (`../Plugins/BSP/BSP_PanZoom`). It is `EnabledByDefault`.

To rebuild (keep the output path short; deep paths exceed the MSVC path limit):

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="<path>\BSP_PanZoom\BSP_PanZoom.uplugin" -Package="C:\BSPPZ" -TargetPlatforms=Win64 -Rocket
```

then copy `Binaries/Win64` from the package into the plugin folder.

## Usage

Look through the shot camera (pilot it, lock the viewport to it, or let Sequencer's camera cuts drive the viewport). Then, with the mouse over that viewport:

| Input | Does |
|---|---|
| tap `\` | toggle between the original and the adjusted view |
| hold `\` + middle-mouse drag | pan: the picture follows the cursor |
| hold `\` + right-mouse drag | zoom: drag right to get closer |
| hold `\` + mouse wheel | zoom in steps |
| `Shift + \` | reset (100% zoom, no pan) |
| Numpad `4` `6` `8` `2` | pan in steps |
| Numpad `+` `-` | zoom in steps |
| Numpad `*` | toggle zoom/pan between the current values and 100% |
| Numpad `.` | lock and follow the selected Control Rig control |

`Alt` + mouse is left to normal Unreal navigation. Holding `\` and dragging turns 2D Pan/Zoom on automatically.

The viewport toolbar has an icon toggle button (its tooltip lists the current shortcuts) and a **Pan/Zoom** dropdown with Zoom %, Pan X, Pan Y, Reset and the lens-shift option. A small overlay in the viewport shows the zoom and which part of the frame you are looking at.

State is per viewport and session-only. Pan/zoom follows Sequencer's live camera cuts: the offset stays layered on top of whichever camera is active.

**Lens shift through cameras** (on by default): when the viewport looks through a camera, pan slides the frame with an off-centre projection, exactly like Maya's film offset, so there is no perspective change at all. In a free viewport there is no camera to shift, so pan tilts the view instead.

## Settings and shortcuts

**Editor Preferences > Plugins > BSP 2D Pan/Zoom**

- **Animation Mode Only** (on by default): outside Animation Mode, `\` and `Shift + \` go to Unreal instead, whose own defaults on those keys are *Hold To Enable Preview Mesh* (the colour-calibrator spheres) and *Cycles Preview Mesh*. The toolbar, dropdown and numpad keys work in every mode.
- **Lens Shift Pan Through Cameras** (on by default).

Every shortcut is rebindable in **Editor Preferences > Keyboard Shortcuts > BSP 2D Pan/Zoom**. To use `\` in every mode and keep the preview mesh, rebind Unreal's two preview-mesh commands (Keyboard Shortcuts > Level Viewport) and turn Animation Mode Only off.

## Python / Blueprint

`unreal.BSPPanZoomLibrary` acts on the viewport under the mouse, else the active one:

```python
import unreal
unreal.BSPPanZoomLibrary.set_zoom(2.0)                     # 200%
unreal.BSPPanZoomLibrary.set_pan(unreal.Vector2D(0.5, 0))  # look a quarter frame-width right
unreal.BSPPanZoomLibrary.reset()
```

Also `get_zoom`, `get_pan`, `set_enabled` and `is_enabled`. Perspective pan is in frame units: X = 1 brings the frame's right edge to the centre, Y = 1 its top edge. Orthographic pan is in world units. Guard calls from other tools with `hasattr(unreal, 'BSPPanZoomLibrary')`.

## How it works

`FEditorViewportClient::CalcSceneView()` broadcasts a `ViewModifiers` delegate before it builds each view (render, picking, gizmos). The plugin hooks it per viewport while active:

- **Zoom** narrows the field of view handed to the modifier, starting from the viewport's `ViewFOV` on every call so repeated views in one frame never compound.
- **Pan through a camera** writes `OffCenterProjectionOffset` on the viewport's protected `ControllingActorViewInfo` (reached via a pointer-to-member in a derived struct, standard C++, no engine change). Unreal refills that struct from the camera every tick, so it is reassigned on every call. Check this member still exists on engine upgrades.
- **Pan in a free viewport** turns the rendered view; the stored viewport camera is untouched.
- **Orthographic** views move the view location and ortho zoom, restored when turned off.

Depth of field is turned off in the viewport while pan/zoom is active and restored afterwards.
