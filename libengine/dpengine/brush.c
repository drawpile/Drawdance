/*
 * Copyright (C) 2022 askmeaboufoom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#include "brush.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <helpers.h> // CLAMP


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


uint16_t DP_classic_brush_soft_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure)
{
    float value = DP_classic_brush_size_at(cb, pressure) * 256.0f + 0.5f;
    return DP_float_to_uint16(CLAMP(value, 0, UINT16_MAX));
}

uint8_t DP_classic_brush_pixel_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure)
{
    float value = DP_classic_brush_size_at(cb, pressure) + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 1.0f, UINT8_MAX));
}

uint8_t DP_classic_brush_dab_opacity_at(const DP_ClassicBrush *cb,
                                        float pressure)
{
    float value = DP_classic_brush_opacity_at(cb, pressure) * 255.0f + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}

uint8_t DP_classic_brush_dab_hardness_at(const DP_ClassicBrush *cb,
                                         float pressure)
{
    float value = DP_classic_brush_hardness_at(cb, pressure) * 255.0f + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}