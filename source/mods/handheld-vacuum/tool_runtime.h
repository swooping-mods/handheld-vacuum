/* Provider-local Unity boundary, shared native item framework integration. */
#include "unity.h"
/* Instantiate the existing safe store once with room for full identity hints.
 * Ownership still supplies its exact 16-byte TSave buffer and schema below. */
#define SS_ROOT_MAX 1000u
#define SS_MAX_DIRS 512u
#define SS_PATH_MAX 1200u
#define SS_MAX_BYTES 1024u
#include "../../runtime/sdk/storage.h"
#undef SS_MAX_BYTES
#include "../equipment-framework/sdk/tool_runtime.h"
