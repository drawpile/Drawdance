/*
 * Copyright (C) 2022 askmeaboutloom
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
#ifndef DPENGINE_BLEND_MODE_H
#define DPENGINE_BLEND_MODE_H
#include <dpcommon/common.h>


#define DP_BLEND_MODE_MAX 255

typedef enum DP_BlendMode {
    DP_BLEND_MODE_ERASE = 0,
    DP_BLEND_MODE_NORMAL,
    DP_BLEND_MODE_MULTIPLY,
    DP_BLEND_MODE_DIVIDE,
    DP_BLEND_MODE_BURN,
    DP_BLEND_MODE_DODGE,
    DP_BLEND_MODE_DARKEN,
    DP_BLEND_MODE_LIGHTEN,
    DP_BLEND_MODE_SUBTRACT,
    DP_BLEND_MODE_ADD,
    DP_BLEND_MODE_RECOLOR,
    DP_BLEND_MODE_BEHIND,
    DP_BLEND_MODE_COLOR_ERASE,
    DP_BLEND_MODE_REPLACE = DP_BLEND_MODE_MAX,
    DP_BLEND_MODE_COUNT,
} DP_BlendMode;

typedef enum DP_BlendModeBlankTileBehavior {
    DP_BLEND_MODE_BLANK_TILE_SKIP,
    DP_BLEND_MODE_BLANK_TILE_BLEND,
} DP_BlendModeBlankTileBehavior;


bool DP_blend_mode_exists(int blend_mode);

bool DP_blend_mode_valid_for_layer(int blend_mode);

bool DP_blend_mode_valid_for_brush(int blend_mode);

const char *DP_blend_mode_enum_name(int blend_mode);

const char *DP_blend_mode_enum_name_unprefixed(int blend_mode);

DP_BlendModeBlankTileBehavior DP_blend_mode_blank_tile_behavior(int blend_mode);


#endif
