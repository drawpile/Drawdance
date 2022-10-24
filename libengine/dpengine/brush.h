#ifndef DPENGINE_BRUSH_H
#define DPENGINE_BRUSH_H
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpmsg/blend_mode.h>
#include <mypaint-brush-settings-gen.h>

#define DP_MYPAINT_CONTROL_POINTS_COUNT 64


typedef enum DP_ClassicBrushShape {
    DP_CLASSIC_BRUSH_SHAPE_PIXEL_ROUND,
    DP_CLASSIC_BRUSH_SHAPE_PIXEL_SQUARE,
    DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND,
} DP_ClassicBrushShape;

typedef struct DP_ClassicBrushRange {
    float min;
    float max;
} DP_ClassicBrushRange;

typedef struct DP_ClassicBrush {
    DP_ClassicBrushRange size;
    DP_ClassicBrushRange hardness;
    DP_ClassicBrushRange opacity;
    DP_ClassicBrushRange smudge;
    float spacing;
    int resmudge;
    DP_UPixelFloat color;
    DP_ClassicBrushShape shape;
    DP_BlendMode mode;
    bool incremental;
    bool colorpick;
    bool size_pressure;
    bool hardness_pressure;
    bool opacity_pressure;
    bool smudge_pressure;
} DP_ClassicBrush;


typedef struct DP_MyPaintControlPoints {
    float xvalues[DP_MYPAINT_CONTROL_POINTS_COUNT];
    float yvalues[DP_MYPAINT_CONTROL_POINTS_COUNT];
    int n;
} DP_MyPaintControlPoints;

typedef struct DP_MyPaintMapping {
    float base_value;
    DP_MyPaintControlPoints inputs[MYPAINT_BRUSH_INPUTS_COUNT];
} DP_MyPaintMapping;

typedef struct DP_MyPaintSettings {
    DP_MyPaintMapping mappings[MYPAINT_BRUSH_SETTINGS_COUNT];
} DP_MyPaintSettings;

typedef struct DP_MyPaintBrush {
    DP_UPixelFloat color;
    bool lock_alpha;
    bool erase;
} DP_MyPaintBrush;


float DP_classic_brush_spacing_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_size_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_hardness_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_opacity_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_smudge_at(const DP_ClassicBrush *cb, float pressure);


#endif
