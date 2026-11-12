#ifndef MOON_EQUIPMENT_MENU_CLEANUP_V1_H
#define MOON_EQUIPMENT_MENU_CLEANUP_V1_H
#define ME_CLEANUP_ABI 1u
#define ME_CLEANUP_LIMIT 32u
/* Separate optional service: MoonEquipment ABI1 stays byte-for-byte unchanged.
 * Register during ModStart on Equipment's first startup worker. Registration
 * never establishes the Unity owner. Keys are copied;
 * callback code must remain resident until process exit, including failed
 * startup. Existing loaders retain modules; hot unload is unsupported.
 * Called on every existing MoonMainMenu.Update, before catalog collection, on
 * Equipment's observed Unity thread. No walker/context/activation is implied.
 * Menu dispatch can interleave registration: a callback may execute immediately
 * after atomic publication, before register_cleanup or ModStart returns.
 * Initialize callback state or install an atomic ready gate BEFORE registering;
 * publish ready only after successful startup and keep failed startup inert.
 * Callbacks must be bounded, nonrecursive, idempotent and retain failed cleanup
 * for retry. Recursive menu dispatch is suppressed by the framework. */
typedef struct {
    unsigned abi,size;
    int (*register_cleanup)(const char *key,void (*callback)(void));
} MoonEquipmentMenuCleanupV1;
typedef const MoonEquipmentMenuCleanupV1 *(*MoonEquipmentMenuCleanupGet)(unsigned abi,unsigned size);
#endif
