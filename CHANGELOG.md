# Changelog

## 1.0 (2026-10-29)

Forked from Loops 2D Pan/Zoom 1.8.0 and renamed BSP_PanZoom (module `BSP_PanZoom`, Python `unreal.BSPPanZoomLibrary`). Supersedes the earlier, never-built BSP_PanZoom hand-off.

- Lens-shift pan through cameras (`OffCenterProjectionOffset`), with a setting to turn it off. Free viewports still tilt.
- Perspective pan stored in frame units: drag follows the cursor exactly, including letterboxed cameras; the overlay crop rectangle is now exact.
- Control Rig lock centres the control exactly with lens shift, and works in orthographic views.
- `UBSPPanZoomLibrary` for Python/Blueprint: set/get zoom and pan, enable, reset.
- Viewport toolbar Pan/Zoom dropdown: Zoom %, Pan X/Y, Reset, lens-shift option.
- Hold `\` + mouse wheel to zoom.
- Removed upstream release automation.

Upstream history: https://github.com/LoopsCreativeStudio/Loops2dPanZoom/blob/main/CHANGELOG.md
