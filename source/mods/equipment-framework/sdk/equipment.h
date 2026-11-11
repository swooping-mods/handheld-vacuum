#ifndef MOON_EQUIPMENT_V1_H
#define MOON_EQUIPMENT_V1_H
#define ME_ABI 1u
#define ME_LIMIT 32
/* Registration is startup-only. Keys and callbacks remain resident until exit.
 * Ownership belongs to the provider and never implies a free native purchase. */
typedef struct {
    unsigned int abi, size;
    const char *key, *name, *description, *template_id;
    int (*owned)(void *world, void *walker);
    int (*configure)(void *native_definition, void *world);
} MoonEquipmentDefinition;
typedef struct {
    unsigned int abi, size;
    int (*register_item)(const MoonEquipmentDefinition *definition);
    int (*is_equipped)(const char *key, void *walker);
    /* Select an existing assigned native slot. Does not replace another item. */
    int (*equip)(const char *key, void *walker);
} MoonEquipment;
typedef const MoonEquipment *(*MoonEquipmentGet)(unsigned int abi, unsigned int size);
#endif
