#ifndef DPENGINE_SAVE_H
#define DPENGINE_SAVE_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;


typedef struct DP_SaveFormat {
    const char *title;
    const char **extensions; // Terminated by a NULL element.
} DP_SaveFormat;

// Returns supported formats for saving, terminated by a {NULL, NULL} element.
const DP_SaveFormat *DP_save_supported_formats(void);


typedef enum DP_SaveResult {
    DP_SAVE_RESULT_SUCCESS,
    DP_SAVE_RESULT_BAD_ARGUMENTS,
    DP_SAVE_RESULT_NO_EXTENSION,
    DP_SAVE_RESULT_UNKNOWN_FORMAT,
    DP_SAVE_RESULT_FLATTEN_ERROR,
    DP_SAVE_RESULT_OPEN_ERROR,
    DP_SAVE_RESULT_WRITE_ERROR,
} DP_SaveResult;

DP_SaveResult DP_save(DP_CanvasState *cs, DP_DrawContext *dc, const char *path);


#endif
