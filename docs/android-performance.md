# Android performance notes

Measured on a Huawei Mate Xs (Mali-G76, Vulkan Forward Mobile, inner panel 2200×2480, 60 Hz),
release build, standing in Northshire.

## Root cause: hidden 3D model frames rendered every frame

**Symptom.** About 4–5 FPS in the world. Frames were a flat ~210 ms. Scripts took ~25 ms and
physics ~6 ms; almost all of the rest was `RenderingServer.draw` waiting on the GPU. The GPU took
~220 ms a frame while the world viewport itself only took ~44 ms.

**Cause.** `WowModelFrame` (`clients/wowgd/ui/wow/wow_model_frame.tscn`) owns a `SubViewport`
with its own 3D world, 4× MSAA and glow. It was set to `render_target_update_mode = 4`
(`UPDATE_ALWAYS`). Every panel that shows a model keeps its frame in the tree while hidden:
character, inspect, dress-up, pet stable, tabard, and the hidden glue screens (login,
character select, character create). In the world, about eight of these rendered every frame,
sized to their on-screen pixels. That added the extra opaque pass and the ~31 ms of glow in the
GPU profile.

**Fix.** `UPDATE_WHEN_VISIBLE` (`render_target_update_mode = 2`). The viewport only renders when
its texture was drawn on screen the frame before, so a hidden panel costs nothing. Visible
frames look the same as before, so desktop rendering does not change. `wow_model_frame.gd`
also sets the mode in `_ready`, so a scene edit cannot bring the old value back.

`UPDATE_WHEN_PARENT_VISIBLE` (3) does **not** work here. "Parent" means the parent viewport,
which is the root viewport, and that one draws every frame.

**Result.**

| | Before | After |
|---|---|---|
| FPS | ~5 | ~21 |
| Frame time | ~210 ms | ~48 ms |
| GPU per frame | ~220 ms | ~45 ms |
| Glow pass | ~31 ms | gone |

`clients/wrathgd/ui/wow/wow_model_frame.tscn` has the same `render_target_update_mode = 4` and
has not been changed yet.

Any new `SubViewport` used for UI should use `UPDATE_WHEN_VISIBLE` or `UPDATE_ONCE`, never
`UPDATE_ALWAYS`. `UnitPortrait` already renders once and stops.

## Other Android-only settings

- 3D at half resolution with bilinear upscale (`shared/game/video_settings.gd`). Forward Mobile
  has no FSR1; requesting it logs an error and falls back.
- World glow is off on Android (`shared/game/world/world_sky.gd`).
- Doodad visibility range is capped at 400 yards on Android (`extension/src/wow_models.cpp`).
- Frame pacing (Swappy) is off on Android; Godot 4.7.2's Swappy fails to present on some drivers.
- Use `OS.get_name() == "Android"` for Android branches. `OS.has_feature("android")` is false at
  runtime in this export.
- Runtime `Image.compress(ASTC)` is not available: the export template has no ASTC encoder
  (`_image_compress_astc_func is null`).

## What is left after the fix

GPU profile after the fix, per frame (~45 ms total):

| Pass | ms |
|---|---|
| Render Opaque | ~23 |
| Setup Sky | ~10.5 |
| Render CanvasItems | ~4 |
| Update GPUParticles | ~3 |
| Tonemap | ~2 |

Scripts take ~19 ms of CPU per frame. The next candidates are the sky radiance update, opaque
geometry (terrain shader, primitives), and the per-frame scripts.

## How to measure

- Run with `--perf` and `shared/game/world/world.gd` prints a `PERF` line every 5 s in the world:
  fps, frame time, script time, draw time, physics, viewport CPU/GPU time, draw calls and
  primitives. On Android, put `--perf` in the Android preset's `command_line/extra_args` for a
  test export (intent extras through the launcher did not reach the engine), then
  `adb logcat -s godot:I | grep PERF`.
- Per-pass GPU timings: set `debug/settings/stdout/print_gpu_profile=true` in `project.godot`
  for a test build. It prints a lot and pushes older lines out of logcat, so read it quickly.
- Presented frame times: `dumpsys SurfaceFlinger --latency '<SurfaceView ... (BLAST)#N layer>'`
  (quote the layer name). `dumpsys gfxinfo` measures the Android UI compositor, not the game.
