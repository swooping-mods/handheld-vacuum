# Handheld Vacuum: behavior and implementation

Describes the exported implementation **0.2.0**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

Buy the $150 Handheld Vacuum in Modded Shop, assign an Equipment slot and hold R while selected. The tool searches an unobstructed six-meter, fifteen-degree cone at most five times per second, inspecting at most 512 capsule views. It invokes the real CollectDustCanister path; native cargo capacity, capsule transfer/removal and replication remain authoritative.

## State and lifecycle

Ownership uses the shared provider sidecar and normal quickslot IDs. The active scan is temporary and stops on unequip or input/context loss. Purchases use checked native debit, persisted grants and refund-on-save-failure policy; uncertain transaction state blocks repurchase. World teardown releases owned model/selection state.

## Integration and configuration

Runtime dependencies: [shop-framework](https://github.com/swooping-mods/shop-framework), [equipment-framework](https://github.com/swooping-mods/equipment-framework), [hints-framework](https://github.com/swooping-mods/hints-framework).

The [module metadata](../source/mods/handheld-vacuum/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Requires `shop-framework.shop` | 1 | at least 56 bytes |
| Requires `equipment-framework.equipment` | 1 | at least 32 bytes |
| Requires `hints-framework.hints` | 1 | at least 24 bytes |

## Implementation sources

- [mod.c](../source/mods/handheld-vacuum/mod.c)
- [tool_runtime.h](../source/mods/handheld-vacuum/tool_runtime.h)

## Verification boundary

This collects dust capsules, not loose ground dust or invented credit rewards. Transfer, full-container refusal, effects and saved-state behavior need their own game checks.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
