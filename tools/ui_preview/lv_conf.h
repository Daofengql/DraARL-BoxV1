#include "../../lib/lv_conf.h"
/* Render the firmware's RGB565 layout with host memory and a manual clock. */
#undef LV_MEM_CUSTOM_INCLUDE
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#undef LV_MEM_CUSTOM_ALLOC
#define LV_MEM_CUSTOM_ALLOC malloc
#undef LV_MEM_CUSTOM_FREE
#define LV_MEM_CUSTOM_FREE free
#undef LV_MEM_CUSTOM_REALLOC
#define LV_MEM_CUSTOM_REALLOC realloc
#undef LV_TICK_CUSTOM
#define LV_TICK_CUSTOM 0
#undef LV_USE_FS_FATFS
#define LV_USE_FS_FATFS 0
