#ifndef DPENGINE_BRUSHPREVIEW_H
#define DPENGINE_BRUSHPREVIEW_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_ClassicBrush DP_ClassicBrush;
typedef struct DP_MyPaintBrush DP_MyPaintBrush;
typedef struct DP_MyPaintSettings DP_MyPaintSettings;


typedef enum DP_BrushPreviewShape {
    DP_BRUSH_PREVIEW_STROKE,
    DP_BRUSH_PREVIEW_LINE,
    DP_BRUSH_PREVIEW_RECTANGLE,
    DP_BRUSH_PREVIEW_ELLIPSE,
    DP_BRUSH_PREVIEW_FLOOD_FILL,
    DP_BRUSH_PREVIEW_FLOOD_ERASE,
} DP_BrushPreviewShape;

typedef struct DP_BrushPreview DP_BrushPreview;

void DP_brush_preview_render_classic(DP_BrushPreview *bp,
                                     const DP_ClassicBrush *brush,
                                     DP_BrushPreviewShape shape);

void DP_brush_preview_render_mypaint(DP_BrushPreview *bp,
                                     const DP_MyPaintBrush *brush,
                                     const DP_MyPaintSettings *settings,
                                     DP_BrushPreviewShape shape);

void DP_classic_brush_preview_dab(const DP_ClassicBrush *cb,
                                  unsigned char *image, int width, int height,
                                  DP_UPixelFloat color);

#endif
