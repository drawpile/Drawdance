#ifndef DPENGINE_BRUSHPREVIEW_H
#define DPENGINE_BRUSHPREVIEW_H
#include <dpcommon/common.h>

typedef struct DP_ClassicBrush DP_ClassicBrush;
typedef struct DP_BrushEngine DP_BrushEngine;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
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

DP_BrushPreview *DP_brush_preview_new(void);

void DP_brush_preview_free(DP_BrushPreview *bp);

void DP_brush_preview_render_classic(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_ClassicBrush *brush,
                                     DP_BrushPreviewShape shape);

void DP_brush_preview_render_mypaint(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_MyPaintBrush *brush,
                                     const DP_MyPaintSettings *settings,
                                     DP_BrushPreviewShape shape);

void DP_brush_preview_render_flood_fill(DP_BrushPreview *bp,
                                        uint32_t fill_color, double tolerance,
                                        int expand, int blend_mode);

DP_Image *DP_brush_preview_to_image(DP_BrushPreview *bp);

DP_Image *DP_classic_brush_preview_dab(const DP_ClassicBrush *cb,
                                       DP_DrawContext *dc, int width,
                                       int height, uint32_t color);

#endif
