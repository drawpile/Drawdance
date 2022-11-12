#ifndef DPENGINE_FLOOD_FILL_H
#define DPENGINE_FLOOD_FILL_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Image DP_Image;


DP_Image *DP_flood_fill(DP_CanvasState *cs, int x, int y, DP_Pixel8 fill_color,
                        double tolerance, int layer_id, bool sample_merged,
                        int size_limit, int expand, int *out_x, int *out_y);


#endif
