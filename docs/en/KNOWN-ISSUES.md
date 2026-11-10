# Known issues and preview limits

[中文](../KNOWN-ISSUES.md) | **English**

This preview is not a stable release and has not established general motion-discomfort relief.

- Exclusive fullscreen can hide external overlays; prefer borderless/windowed mode. Compatibility and game/anti-cheat rules vary.
- Low texture, blur, cuts and weapon/HUD occlusion can reduce optical-flow quality. Input prediction estimates intent, not actual jumping, walking into a wall, driving or cinematic motion.
- Dynamic styles do not automatically switch to Static. With insufficient observations, the input/dynamic path remains and a waiting/quality status may appear.
- Controls may still flicker or resize incorrectly in some display environments. Include the theme, display scaling and reproduction steps when reporting a problem.
- Multi-monitor and all DPI scales are not fully validated. Maximize remains disabled.
- Full Tab navigation is incomplete; mouse operation is the primary path.
- Concurrent versions can compete for global shortcuts. Close other versions first; conflicts are indicated and retried.
- Logs require a writable directory beside the EXE. Failure is reported without silently writing elsewhere.
- The grayscale OpenCV backend exists. NVIDIA optical flow, RGB flow, AI segmentation/prediction and official game telemetry are not implemented.
- Only the Windows/UCRT64 build is validated. MSVC, different GPUs and extended game sessions need broader testing.
