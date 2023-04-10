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
#include "view_mode.h"
#include "canvas_state.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "timeline.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define TYPE_NORMAL          0
#define TYPE_NOTHING         1
#define TYPE_LAYER           2
#define TYPE_FRAME_MANUAL    3
#define TYPE_FRAME_AUTOMATIC 4


static DP_ViewModeFilter make_normal(void)
{
    return (DP_ViewModeFilter){TYPE_NORMAL, {0}};
}

static DP_ViewModeFilter make_nothing(void)
{
    return (DP_ViewModeFilter){TYPE_NOTHING, {0}};
}

static DP_ViewModeFilter make_layer(int layer_id)
{
    return (DP_ViewModeFilter){TYPE_LAYER, {.layer_id = layer_id}};
}

static DP_ViewModeFilter make_frame_manual(DP_Frame *f)
{
    return (DP_ViewModeFilter){TYPE_FRAME_MANUAL, {.frame = f}};
}

static DP_ViewModeFilter make_frame_automatic(int layer_id)
{
    return (DP_ViewModeFilter){TYPE_FRAME_AUTOMATIC, {.layer_id = layer_id}};
}

static DP_ViewModeFilter build_frame_manual(DP_CanvasState *cs, int frame_index)
{
    // FIXME
    (void)cs;
    (void)frame_index;
    return make_nothing();
}

static DP_ViewModeFilter build_frame_automatic(DP_CanvasState *cs,
                                               int frame_index)
{
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int layer_count = DP_layer_props_list_count(lpl);
    if (frame_index >= 0 && frame_index < layer_count) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, frame_index);
        return make_frame_automatic(DP_layer_props_id(lp));
    }
    else {
        return make_nothing();
    }
}

DP_ViewModeFilter DP_view_mode_filter_make_default(void)
{
    return make_normal();
}

DP_ViewModeFilter DP_view_mode_filter_make_frame(DP_CanvasState *cs,
                                                 int frame_index)
{
    DP_ASSERT(cs);
    return DP_canvas_state_use_timeline(cs)
             ? build_frame_manual(cs, frame_index)
             : build_frame_automatic(cs, frame_index);
}

DP_ViewModeFilter DP_view_mode_filter_make(DP_ViewMode vm, DP_CanvasState *cs,
                                           int layer_id, int frame_index)
{
    switch (vm) {
    case DP_VIEW_MODE_NORMAL:
        return make_normal();
    case DP_VIEW_MODE_LAYER:
        return make_layer(layer_id);
    case DP_VIEW_MODE_FRAME:
        return DP_view_mode_filter_make_frame(cs, frame_index);
    default:
        DP_UNREACHABLE();
    }
}


bool DP_view_mode_filter_excludes_everything(const DP_ViewModeFilter *vmf)
{
    DP_ASSERT(vmf);
    return vmf->internal_type == TYPE_NOTHING;
}


static DP_ViewModeFilterResult make_result(bool hidden_by_view_mode,
                                           DP_ViewModeFilter child_vmf)
{
    return (DP_ViewModeFilterResult){hidden_by_view_mode, child_vmf};
}

static DP_ViewModeFilterResult apply_layer(int layer_id, DP_LayerProps *lp)
{
    if (DP_layer_props_id(lp) == layer_id) {
        return make_result(false, make_normal());
    }
    else if (DP_layer_props_children_noinc(lp)) {
        return make_result(false, make_layer(layer_id));
    }
    else {
        return make_result(true, make_nothing());
    }
}

static DP_ViewModeFilterResult apply_frame_manual(DP_Frame *f,
                                                  DP_LayerProps *lp)
{
    // A timeline frame only applies to layers and isolated groups at the
    // top-level or inside non-isolated (pass-through) groups. The frame might
    // contain other, invalid layer ids, which we disregard.
    bool is_pass_through_group =
        DP_layer_props_children_noinc(lp) && !DP_layer_props_isolated(lp);
    if (is_pass_through_group) {
        return make_result(false, make_frame_manual(f));
    }
    // FIXME
    // else if (DP_frame_layer_ids_contain(f, DP_layer_props_id(lp))) {
    //     return make_result(false, make_normal());
    // }
    else {
        return make_result(true, make_nothing());
    }
}

static DP_ViewModeFilterResult apply_frame_automatic(int layer_id,
                                                     DP_LayerProps *lp)
{
    // Unlike for the manual timeline, the automatic timeline treats isolated
    // and pass-through the same. Maybe we should change that, since it's silly.
    if (DP_layer_props_id(lp) == layer_id) {
        return make_result(false, make_normal());
    }
    else {
        return make_result(true, make_nothing());
    }
}

DP_ViewModeFilterResult DP_view_mode_filter_apply(const DP_ViewModeFilter *vmf,
                                                  DP_LayerProps *lp)
{
    DP_ASSERT(vmf);
    DP_ASSERT(lp);
    switch (vmf->internal_type) {
    case TYPE_NORMAL:
        return make_result(false, make_normal());
    case TYPE_NOTHING:
        return make_result(true, make_nothing());
    case TYPE_LAYER:
        return apply_layer(vmf->layer_id, lp);
    case TYPE_FRAME_MANUAL:
        return apply_frame_manual(vmf->frame, lp);
    case TYPE_FRAME_AUTOMATIC:
        return apply_frame_automatic(vmf->layer_id, lp);
    default:
        DP_UNREACHABLE();
    }
}


struct DP_OnionSkins {
    int count_below;
    int count_above;
    DP_OnionSkin skins[];
};

DP_OnionSkins *DP_onion_skins_new(int count_below, int count_above)
{
    DP_ASSERT(count_below >= 0);
    DP_ASSERT(count_above >= 0);
    DP_OnionSkins *oss = DP_malloc_zeroed(DP_FLEX_SIZEOF(
        DP_OnionSkins, skins, DP_int_to_size(count_below + count_above)));
    oss->count_below = count_below;
    oss->count_above = count_above;
    return oss;
}

void DP_onion_skins_free(DP_OnionSkins *oss)
{
    DP_free(oss);
}

int DP_onion_skins_count_below(const DP_OnionSkins *oss)
{
    DP_ASSERT(oss);
    return oss->count_below;
}

int DP_onion_skins_count_above(const DP_OnionSkins *oss)
{
    DP_ASSERT(oss);
    return oss->count_above;
}

const DP_OnionSkin *DP_onion_skins_skin_below_at(const DP_OnionSkins *oss,
                                                 int index)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_below);
    return &oss->skins[index];
}

const DP_OnionSkin *DP_onion_skins_skin_above_at(const DP_OnionSkins *oss,
                                                 int index)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_above);
    return &oss->skins[oss->count_below + index];
}

void DP_onion_skins_skin_below_at_set(DP_OnionSkins *oss, int index,
                                      uint16_t opacity, DP_UPixel15 tint)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_below);
    DP_ASSERT(opacity <= DP_BIT15);
    DP_ASSERT(tint.b <= DP_BIT15);
    DP_ASSERT(tint.g <= DP_BIT15);
    DP_ASSERT(tint.r <= DP_BIT15);
    DP_ASSERT(tint.a <= DP_BIT15);
    oss->skins[index] = (DP_OnionSkin){opacity, tint};
}

void DP_onion_skins_skin_above_at_set(DP_OnionSkins *oss, int index,
                                      uint16_t opacity, DP_UPixel15 tint)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_above);
    DP_ASSERT(opacity <= DP_BIT15);
    DP_ASSERT(tint.b <= DP_BIT15);
    DP_ASSERT(tint.g <= DP_BIT15);
    DP_ASSERT(tint.r <= DP_BIT15);
    DP_ASSERT(tint.a <= DP_BIT15);
    oss->skins[oss->count_below + index] = (DP_OnionSkin){opacity, tint};
}
