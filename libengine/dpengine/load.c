#include "load.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "image.h"
#include "layer_content.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "ops.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpmsg/blend_mode.h>


const DP_LoadFormat *DP_load_supported_formats(void)
{
    static const char *png_ext[] = {"png", NULL};
    static const DP_LoadFormat formats[] = {
        {"PNG", png_ext},
        {NULL, NULL},
    };
    return formats;
}


static void assign_load_result(DP_LoadResult *out_result, DP_LoadResult result)
{
    if (out_result) {
        *out_result = result;
    }
}


DP_CanvasState *load_flat_image(DP_Input *input,
                                const char *flat_image_layer_title,
                                DP_LoadResult *out_result)
{
    DP_ImageFileType type;
    DP_Image *img =
        DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS, &type);
    if (!img) {
        assign_load_result(out_result, type == DP_IMAGE_FILE_TYPE_UNKNOWN
                                           ? DP_LOAD_RESULT_UNKNOWN_FORMAT
                                           : DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_background_tile_set_noinc(
        tcs, DP_tile_new_from_bgra(0, 0xffffffffu));

    int width = DP_image_width(img);
    int height = DP_image_height(img);
    DP_transient_canvas_state_width_set(tcs, width);
    DP_transient_canvas_state_height_set(tcs, height);

    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(width, height, NULL);
    DP_transient_layer_content_put_image(tlc, 0, DP_BLEND_MODE_REPLACE, 0, 0,
                                         img);
    DP_image_free(img);

    DP_TransientLayerList *tll =
        DP_transient_canvas_state_transient_layers(tcs, 1);
    DP_transient_layer_list_insert_transient_content_noinc(tll, tlc, 0);

    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init(0x100, false);
    const char *title =
        flat_image_layer_title ? flat_image_layer_title : "Layer 1";
    DP_transient_layer_props_title_set(tlp, title, strlen(title));

    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 1);
    DP_transient_layer_props_list_insert_transient_noinc(tlpl, tlp, 0);

    // TODO this pointlessly allocates a bunch of memory for brush masks.
    DP_DrawContext *dc = DP_draw_context_new();
    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    DP_draw_context_free(dc);

    assign_load_result(out_result, DP_LOAD_RESULT_SUCCESS);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_load(const char *path, const char *flat_image_layer_title,
                        DP_LoadResult *out_result)
{
    if (!path) {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        return NULL;
    }

    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return NULL;
    }

    DP_CanvasState *cs =
        load_flat_image(input, flat_image_layer_title, out_result);

    DP_input_free(input);
    return cs;
}
