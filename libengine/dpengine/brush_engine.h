#ifndef DPENGINE_BRUSHENGINE_H
#define DPENGINE_BRUSHENGINE_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_ClassicBrush DP_ClassicBrush;
typedef struct DP_Message DP_Message;
typedef struct DP_MyPaintBrush DP_MyPaintBrush;
typedef struct DP_MyPaintSettings DP_MyPaintSettings;


typedef void (*DP_BrushEnginePushMessageFn)(void *user, DP_Message *msg);

typedef struct DP_BrushEngine DP_BrushEngine;

DP_BrushEngine *DP_brush_engine_new(DP_BrushEnginePushMessageFn push_message,
                                    void *user);

void DP_brush_engine_free(DP_BrushEngine *be);

void DP_brush_engine_classic_brush_set(DP_BrushEngine *be,
                                       const DP_ClassicBrush *brush,
                                       int layer_id);

void DP_brush_engine_mypaint_brush_set(DP_BrushEngine *be,
                                       const DP_MyPaintBrush *brush,
                                       const DP_MyPaintSettings *settings,
                                       int layer_id, bool freehand);

void DP_brush_engine_dabs_flush(DP_BrushEngine *be);

// Sets the context id for this stroke and pushes an undo point message.
void DP_brush_engine_stroke_begin(DP_BrushEngine *be, unsigned int context_id);

// Pushes draw dabs messages.
void DP_brush_engine_stroke_to(DP_BrushEngine *be, float x, float y,
                               float pressure, long long delta_msec,
                               DP_CanvasState *cs_or_null);

// Pushes a pen up message.
void DP_brush_engine_stroke_end(DP_BrushEngine *be);

void DP_brush_engine_offset_add(DP_BrushEngine *be, float x, float y);


#endif
