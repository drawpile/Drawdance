#include "brush.h"
#include <dpcommon/common.h>


static float lerp_range(const DP_ClassicBrushRange *cbr, float pressure)
{
    float min = cbr->min;
    return (cbr->max - min) * pressure + min;
}

static float lerp_range_if(const DP_ClassicBrushRange *cbr, float pressure,
                           bool condition)
{
    return condition ? lerp_range(cbr, pressure) : cbr->max;
}

float DP_classic_brush_spacing_at(const DP_ClassicBrush *cb, float pressure)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return cb->spacing * DP_classic_brush_size_at(cb, pressure);
}

float DP_classic_brush_size_at(const DP_ClassicBrush *cb, float pressure)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_if(&cb->size, pressure, cb->size_pressure);
}

float DP_classic_brush_hardness_at(const DP_ClassicBrush *cb, float pressure)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_if(&cb->hardness, pressure, cb->hardness_pressure);
}

float DP_classic_brush_opacity_at(const DP_ClassicBrush *cb, float pressure)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_if(&cb->opacity, pressure, cb->opacity_pressure);
}

float DP_classic_brush_smudge_at(const DP_ClassicBrush *cb, float pressure)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_if(&cb->smudge, pressure, cb->smudge_pressure);
}
