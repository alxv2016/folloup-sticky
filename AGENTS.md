# Agent Notes

Read `docs/app-architecture.md` before changing the firmware architecture,
component boundaries, BQ27220 integration, board wiring, partition layout, or
ESP-IDF configuration.

Keep `main/app_shell.cpp` as an orchestration layer. Before adding logic there,
ask whether the behavior belongs in a service/component or a focused runtime
helper instead. `app_shell` may own startup ordering, event wiring, simple
product-level routing, and policy composition, but it should not grow hardware
driver logic, protocol logic, display drawing, power/sleep mechanics, long-lived
feature loops, or business logic that a service/component should own.

Do not automatically run builds in this repo. If the user explicitly instructs
you to run a build, use the existing `build/` folder and do not create a new
build folder.
