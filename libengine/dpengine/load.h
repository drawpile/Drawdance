#ifndef DPENGINE_LOAD_H
#define DPENGINE_LOAD_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;


typedef struct DP_LoadFormat {
    const char *title;
    const char **extensions; // Terminated by a NULL element.
} DP_LoadFormat;

// Returns supported formats for loading, terminated by a {NULL, NULL} element.
const DP_LoadFormat *DP_load_supported_formats(void);


typedef enum DP_LoadResult {
    DP_LOAD_RESULT_SUCCESS,
    DP_LOAD_RESULT_BAD_ARGUMENTS,
    DP_LOAD_RESULT_UNKNOWN_FORMAT,
    DP_LOAD_RESULT_OPEN_ERROR,
    DP_LOAD_RESULT_READ_ERROR,
} DP_LoadResult;

DP_CanvasState *DP_load(DP_DrawContext *dc, const char *path,
                        const char *flat_image_layer_title,
                        DP_LoadResult *out_result);


#endif
