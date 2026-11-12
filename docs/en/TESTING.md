# Testing and feedback

[中文](../TESTING.md) | **English**

## Software checks

`build.ps1` runs CTest after compilation. Checks cover preferences, the input/flow pipeline, scrolling/reversal continuity, preview, themed controls, log rotation, guide display and packaging. They do not establish game comfort efficacy or universal freedom from flicker.

The build directory's `desktop_smoke.exe` runs static, regional and edge self-capture checks on an interactive Windows desktop in about 20 seconds, without game input. It explicitly enables diagnostic logs and writes `desktop-smoke-results.txt` plus `logs/`. Normal startup still defaults to logging off.

`MotionCompensator.exe --smoke-test` is one approximately six-second self-capture check. It requires an available interactive Windows desktop; capture may fail in restricted or noninteractive environments.

## UI regression

1. Check light, dark and system themes, dropdowns, sliders, buttons and document scrolling.
2. Cycle Regional → Edge → Static → Regional; inspect background-adaptation controls and independent saved settings.
3. Minimize/restore, resize live, open/scroll/close instructions; watch for missing paint, persistent ghosts and white borders.
4. Verify Licenses & credits contains the complete project license, design/video links and third-party notices.
5. Verify logging starts only when enabled and stops when disabled; an unwritable directory should report failure without making the app unusable.
6. UI themes must not change game-dot/reticle colors. Restore initial settings afterward.

## Game comparisons

The user performs game testing, preferably in a familiar PAYDAY 2 single-player scene. Hold resolution, FOV, frame cap and sensitivity constant. Compare app off, app running with overlay hidden, Static, Regional and Edge.

Use short runs along the same route. Record distraction, occlusion, dot continuity, subjective discomfort from 0–10 and onset time. Stop on discomfort. Short subjective improvements are not a universal or medical conclusion.

Reports should include app/Windows/game versions, resolution/scaling, monitor setup, window mode, input device, style/settings and reproduction steps. Logs are optional and should be reviewed for private data first.

Capture/analysis/submission FPS is not game FPS. CPU submission time is not GPU completion or display latency. Unavailable metrics must not be guessed or filled with zero.

Slider and scrolling regression: drag rapidly in both directions, release outside the track, then fine-tune with arrow keys. Check thumb, label and final preview agreement. Verify values after profile switches and restart. The wheel should scroll the settings content instead of changing slider values; page navigation and preview should stay fixed.
