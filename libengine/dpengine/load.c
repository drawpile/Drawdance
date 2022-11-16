#include "load.h"
#include "canvas_state.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "ops.h"
#include "tile.h"
#include "xml_stream.h"
#include "zip_archive.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpmsg/blend_mode.h>
#include <ctype.h>


const DP_LoadFormat *DP_load_supported_formats(void)
{
    static const char *ora_ext[] = {"ora", NULL};
    static const char *png_ext[] = {"png", NULL};
    static const DP_LoadFormat formats[] = {
        {"OpenRaster", ora_ext},
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


typedef enum DP_ReadOraExpect {
    DP_READ_ORA_EXPECT_IMAGE,
    DP_READ_ORA_EXPECT_ROOT_STACK,
    DP_READ_ORA_EXPECT_STACK_OR_LAYER,
    DP_READ_ORA_EXPECT_LAYER_END,
    DP_READ_ORA_EXPECT_END,
} DP_ReadOraExpect;

typedef struct DP_ReadOraGroup {
    DP_TransientLayerGroup *tlg;
    DP_TransientLayerProps *tlp;
    int child_count;
} DP_ReadOraGroup;

typedef struct DP_ReadOraChildren {
    union {
        DP_TransientLayerContent *tlc;
        DP_TransientLayerGroup *tlg;
    };
    DP_TransientLayerProps *tlp;
} DP_ReadOraChildren;

typedef struct DP_ReadOraContext {
    DP_DrawContext *dc;
    DP_ZipReader *zr;
    DP_Worker *worker;
    DP_ReadOraExpect expect;
    DP_TransientCanvasState *tcs;
    int next_id;
    int garbage_depth;
    DP_Queue groups;
    DP_Queue children;
} DP_ReadOraContext;

static bool ora_check_mimetype(DP_ZipReader *zr)
{
    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(zr, "mimetype");
    if (!zrf) {
        DP_error_set("No mimetype file found in archive");
        return false;
    }

    size_t size = DP_zip_reader_file_size(zrf);
    const char *content = DP_zip_reader_file_content(zrf);
    // Skip leading whitespace, we allow that kinda thing.
    size_t i = 0;
    while (i < size && isspace(content[i])) {
        ++i;
    }
    // Actually compare the mime type.
    const char *mimetype = "image/openraster";
    size_t len = strlen(mimetype);
    if (size - i < len || memcmp(content + i, mimetype, len) != 0) {
        DP_error_set("Incorrect mime type");
        DP_zip_reader_file_free(zrf);
        return false;
    }
    // Skip trailing whitespace too, especially a trailing newline is likely.
    i += len;
    while (i < size && isspace(content[i])) {
        ++i;
    }
    DP_zip_reader_file_free(zrf);
    if (i == size) {
        return true;
    }
    else {
        DP_error_set("Garbage after mimetype");
        DP_zip_reader_file_free(zrf);
        return false;
    }
}

static bool ora_read_int_attribute(DP_XmlElement *element, const char *name,
                                   long min, long max, int *out_value)
{
    const char *s = DP_xml_element_attribute(element, name);
    if (s) {
        char *end;
        long value = strtol(s, &end, 10);
        if (*end == '\0' && value >= min && value <= max) {
            *out_value = DP_long_to_int(value);
            return true;
        }
    }
    return false;
}

static bool ora_read_float_attribute(DP_XmlElement *element, const char *name,
                                     float min, float max, float *out_value)
{
    const char *s = DP_xml_element_attribute(element, name);
    if (s) {
        char *end;
        float value = strtof(s, &end);
        if (*end == '\0' && value >= min && value <= max) {
            *out_value = value;
            return true;
        }
    }
    return false;
}

static void push_group(DP_ReadOraContext *c, DP_TransientLayerGroup *tlg,
                       DP_TransientLayerProps *tlp)
{
    DP_ReadOraGroup *rog = DP_queue_push(&c->groups, sizeof(*rog));
    *rog = (DP_ReadOraGroup){tlg, tlp, 0};
}

static void push_layer_children(DP_ReadOraContext *c, DP_ReadOraChildren roc)
{
    DP_ReadOraGroup *rog = DP_queue_peek_last(&c->groups, sizeof(*rog));
    ++rog->child_count;
    *(DP_ReadOraChildren *)DP_queue_push(&c->children, sizeof(roc)) = roc;
}

static bool ora_handle_image(DP_ReadOraContext *c, DP_XmlElement *element)
{
    int width;
    if (!ora_read_int_attribute(element, "w", 1, INT16_MAX, &width)) {
        DP_error_set("Invalid width");
        return false;
    }

    int height;
    if (!ora_read_int_attribute(element, "h", 1, INT16_MAX, &height)) {
        DP_error_set("Invalid height");
        return false;
    }

    c->tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_width_set(c->tcs, width);
    DP_transient_canvas_state_height_set(c->tcs, height);

    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(c->tcs);

    int dpix;
    if (ora_read_int_attribute(element, "xres", 1, INT32_MAX, &dpix)) {
        DP_transient_document_metadata_dpix_set(tdm, dpix);
    }

    int dpiy;
    if (ora_read_int_attribute(element, "yres", 1, INT32_MAX, &dpiy)) {
        DP_transient_document_metadata_dpiy_set(tdm, dpiy);
    }

    int framerate;
    if (ora_read_int_attribute(element, "drawpile:framerate", 1, INT32_MAX,
                               &framerate)) {
        DP_transient_document_metadata_framerate_set(tdm, framerate);
    }

    push_group(c, NULL, NULL);
    c->expect = DP_READ_ORA_EXPECT_ROOT_STACK;
    return true;
}

static int ora_get_next_id(DP_ReadOraContext *c)
{
    int layer_id = c->next_id++;
    if (layer_id <= UINT16_MAX) {
        return layer_id;
    }
    else {
        DP_error_set("Out of ids");
        return -1;
    }
}

static DP_TransientLayerProps *ora_make_layer_props(DP_XmlElement *element,
                                                    int layer_id, bool group)
{
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init(layer_id, group);

    float opacity;
    if (ora_read_float_attribute(element, "opacity", 0.0f, 1.0f, &opacity)) {
        DP_transient_layer_props_opacity_set(
            tlp, DP_float_to_uint16(opacity * (float)DP_BIT15 + 0.5f));
    }

    const char *title = DP_xml_element_attribute(element, "name");
    if (title) {
        DP_transient_layer_props_title_set(tlp, title, strlen(title));
    }

    const char *visibility = DP_xml_element_attribute(element, "visibility");
    if (DP_str_equal_lowercase(visibility, "hidden")) {
        DP_transient_layer_props_hidden_set(tlp, true);
    }

    DP_BlendMode blend_mode = DP_blend_mode_by_svg_name(
        DP_xml_element_attribute(element, "composite-op"),
        DP_BLEND_MODE_NORMAL);
    DP_transient_layer_props_blend_mode_set(tlp, (int)blend_mode);

    const char *censored =
        DP_xml_element_attribute(element, "drawpile:censored");
    if (DP_str_equal_lowercase(censored, "true")) {
        DP_transient_layer_props_censored_set(tlp, true);
    }

    return tlp;
}

static void free_zip_reader_file(DP_UNUSED void *buffer, DP_UNUSED size_t size,
                                 void *free_arg)
{
    DP_ZipReaderFile *zrf = free_arg;
    DP_zip_reader_file_free(zrf);
}

static DP_Image *ora_load_png_free(DP_ZipReaderFile *zrf)
{
    DP_Input *input = DP_mem_input_new(DP_zip_reader_file_content(zrf),
                                       DP_zip_reader_file_size(zrf),
                                       free_zip_reader_file, zrf);
    DP_Image *img = DP_image_read_png(input);
    DP_input_free(input);
    return img;
}

static bool ora_try_load_background_tile(DP_ReadOraContext *c,
                                         DP_XmlElement *element)
{
    const char *path =
        DP_xml_element_attribute(element, "mypaint:background-tile");
    if (!path) {
        return false;
    }

    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, path);
    if (!zrf) {
        DP_warn("Background tile '%s' not found in archive", path);
        return false;
    }

    DP_Image *img = ora_load_png_free(zrf);
    if (!img) {
        DP_warn("Could not read background tile from '%s': %s", path,
                DP_error());
        return false;
    }

    int width = DP_image_width(img);
    int height = DP_image_height(img);
    if (width != DP_TILE_SIZE || height != DP_TILE_SIZE) {
        DP_warn("Invalid background tile dimensions %dx%d", width, height);
        DP_image_free(img);
        return false;
    }

    DP_transient_canvas_state_background_tile_set_noinc(
        c->tcs, DP_tile_new_from_pixels8(0, DP_image_pixels(img)));
    DP_image_free(img);
    return true;
}

struct DP_OraLoadLayerContentParams {
    DP_TransientLayerContent *tlc;
    DP_ZipReaderFile *zrf;
    int x, y;
};

static void ora_load_layer_content_job(void *user)
{
    struct DP_OraLoadLayerContentParams *params = user;
    DP_TransientLayerContent *tlc = params->tlc;
    DP_ZipReaderFile *zrf = params->zrf;
    int x = params->x;
    int y = params->y;
    DP_free(params);
    DP_Image *img = ora_load_png_free(zrf);
    if (img) {
        DP_transient_layer_content_put_image(tlc, 0, DP_BLEND_MODE_REPLACE, x,
                                             y, img);
        DP_image_free(img);
    }
    else {
        DP_warn("Error reading ORA layer content: %s", DP_error());
    }
}

static void ora_load_layer_content_in_worker(DP_ReadOraContext *c,
                                             DP_XmlElement *element,
                                             DP_TransientLayerContent *tlc)
{
    const char *src = DP_xml_element_attribute(element, "src");
    if (!src) {
        return;
    }

    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, src);
    if (!zrf) {
        DP_warn("ORA source '%s' not found in archive", src);
        return;
    }

    struct DP_OraLoadLayerContentParams *params = DP_malloc(sizeof(*params));
    *params = (struct DP_OraLoadLayerContentParams){tlc, zrf, 0, 0};
    ora_read_int_attribute(element, "x", INT32_MIN, INT32_MAX, &params->x);
    ora_read_int_attribute(element, "y", INT32_MIN, INT32_MAX, &params->y);
    DP_worker_push(c->worker, ora_load_layer_content_job, params);
}

static bool ora_handle_layer(DP_ReadOraContext *c, DP_XmlElement *element)
{
    if (ora_try_load_background_tile(c, element)) {
        return true;
    }

    int layer_id = ora_get_next_id(c);
    if (layer_id == -1) {
        return false;
    }

    DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
        DP_transient_canvas_state_width(c->tcs),
        DP_transient_canvas_state_height(c->tcs), NULL);
    // Layer content loading is slow, so we do it asynchronously in a worker.
    // These workers are joined before the canvas state is persisted/freed.
    ora_load_layer_content_in_worker(c, element, tlc);

    DP_TransientLayerProps *tlp =
        ora_make_layer_props(element, layer_id, false);

    push_layer_children(c, (DP_ReadOraChildren){.tlc = tlc, .tlp = tlp});
    c->expect = DP_READ_ORA_EXPECT_LAYER_END;
    return true;
}

static bool ora_handle_stack(DP_ReadOraContext *c, DP_XmlElement *element)
{
    int layer_id = ora_get_next_id(c);
    if (layer_id == -1) {
        return false;
    }

    DP_TransientLayerGroup *tlg = DP_transient_layer_group_new_init(
        DP_transient_canvas_state_width(c->tcs),
        DP_transient_canvas_state_height(c->tcs), 0);

    DP_TransientLayerProps *tlp = ora_make_layer_props(element, layer_id, true);
    const char *isolation = DP_xml_element_attribute(element, "isolation");
    if (!DP_str_equal_lowercase(isolation, "isolate")) {
        DP_transient_layer_props_isolated_set(tlp, false);
    }

    push_layer_children(c, (DP_ReadOraChildren){.tlg = tlg, .tlp = tlp});
    push_group(c, tlg, tlp);
    return true;
}

static bool ora_handle_stack_end(DP_ReadOraContext *c)
{
    DP_ReadOraGroup *rog = DP_queue_peek_last(&c->groups, sizeof(*rog));
    int count = rog->child_count;

    DP_TransientLayerList *tll;
    DP_TransientLayerPropsList *tlpl;
    if (rog->tlg) {
        DP_ASSERT(rog->tlp);
        tll = DP_transient_layer_group_transient_children(rog->tlg, count);
        tlpl = DP_transient_layer_props_transient_children(rog->tlp, count);
    }
    else {
        DP_ASSERT(!rog->tlp);
        tll = DP_transient_canvas_state_transient_layers(c->tcs, count);
        tlpl = DP_transient_canvas_state_transient_layer_props(c->tcs, count);
        c->expect = DP_READ_ORA_EXPECT_END;
    }

    for (int i = 0; i < count; ++i) {
        DP_ReadOraChildren *roc =
            DP_queue_peek_last(&c->children, sizeof(*roc));
        DP_TransientLayerProps *tlp = roc->tlp;
        if (DP_transient_layer_props_children_noinc(tlp)) {
            DP_transient_layer_list_insert_transient_group_noinc(tll, roc->tlg,
                                                                 i);
        }
        else {
            DP_transient_layer_list_insert_transient_content_noinc(tll,
                                                                   roc->tlc, i);
        }
        DP_transient_layer_props_list_insert_transient_noinc(tlpl, tlp, i);
        DP_queue_pop(&c->children);
    }

    DP_queue_pop(&c->groups);
    return true;
}

static bool ora_xml_start_element(void *user, DP_XmlElement *element)
{
    DP_ReadOraContext *c = user;
    const char *name = DP_xml_element_name(element);

    if (c->garbage_depth != 0) {
        ++c->garbage_depth;
        DP_debug("Garbage element <%s> depth %d", name, c->garbage_depth);
        return true;
    }

    switch (c->expect) {
    case DP_READ_ORA_EXPECT_IMAGE:
        if (DP_str_equal_lowercase(name, "image")) {
            return ora_handle_image(c, element);
        }
        else {
            DP_debug("Expected <image>, got <%s>", name);
            return true;
        }
    case DP_READ_ORA_EXPECT_ROOT_STACK:
        if (DP_str_equal_lowercase(name, "stack")) {
            c->expect = DP_READ_ORA_EXPECT_STACK_OR_LAYER;
            c->next_id = 1;
            return true;
        }
        else {
            DP_debug("Expected root <stack>, got <%s>", name);
            return true;
        }
    case DP_READ_ORA_EXPECT_STACK_OR_LAYER:
        if (DP_str_equal_lowercase(name, "layer")) {
            return ora_handle_layer(c, element);
        }
        else if (DP_str_equal_lowercase(name, "stack")) {
            return ora_handle_stack(c, element);
        }
        else {
            DP_debug("Expected <stack> or <layer>, got <%s>", name);
            ++c->garbage_depth;
            return true;
        }
    case DP_READ_ORA_EXPECT_LAYER_END:
        DP_debug("Expected </layer>, got <%s>", name);
        ++c->garbage_depth;
        break;
    case DP_READ_ORA_EXPECT_END:
        DP_debug("Expected nothing, got <%s>", name);
        return true;
    }
    DP_UNREACHABLE();
}

static bool ora_xml_text_content(void *user, size_t len, const char *text)
{
    (void)user;
    (void)len;
    (void)text;
    return true;
}

static bool ora_xml_end_element(void *user)
{
    DP_ReadOraContext *c = user;

    if (c->garbage_depth != 0) {
        --c->garbage_depth;
        DP_debug("Closing garbage element depth %d", c->garbage_depth);
        return true;
    }

    switch (c->expect) {
    case DP_READ_ORA_EXPECT_STACK_OR_LAYER:
        return ora_handle_stack_end(c);
    case DP_READ_ORA_EXPECT_LAYER_END:
        c->expect = DP_READ_ORA_EXPECT_STACK_OR_LAYER;
        return true;
    case DP_READ_ORA_EXPECT_IMAGE:
    case DP_READ_ORA_EXPECT_ROOT_STACK:
    case DP_READ_ORA_EXPECT_END:
        return true;
    }
    DP_UNREACHABLE();
}

static void dispose_child(void *element, DP_UNUSED void *user)
{
    DP_ReadOraChildren *roc = element;
    DP_TransientLayerProps *tlp = roc->tlp;
    if (DP_transient_layer_props_children_noinc(tlp)) {
        DP_transient_layer_group_decref(roc->tlg);
    }
    else {
        DP_transient_layer_content_decref(roc->tlc);
    }
    DP_transient_layer_props_decref(tlp);
}

static DP_CanvasState *ora_read_stack_xml(DP_ReadOraContext *c)
{
    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, "stack.xml");
    if (!zrf) {
        return NULL;
    }

    c->worker = DP_worker_new(64, DP_thread_cpu_count());
    if (!c->worker) {
        DP_zip_reader_file_free(zrf);
        return NULL;
    }

    bool xml_ok = DP_xml_stream(
        DP_zip_reader_file_size(zrf), DP_zip_reader_file_content(zrf),
        ora_xml_start_element, ora_xml_text_content, ora_xml_end_element, c);
    DP_zip_reader_file_free(zrf);

    if (xml_ok) {
        DP_transient_canvas_state_layer_routes_reindex(c->tcs, c->dc);
        DP_worker_free_join(c->worker);
        return DP_transient_canvas_state_persist(c->tcs);
    }
    else {
        DP_worker_free_join(c->worker);
        DP_transient_canvas_state_decref_nullable(c->tcs);
        DP_queue_each(&c->children, sizeof(DP_ReadOraChildren), dispose_child,
                      NULL);
        return NULL;
    }
}

static DP_CanvasState *load_ora(DP_DrawContext *dc, const char *path,
                                DP_LoadResult *out_result)
{
    DP_ZipReader *zr = DP_zip_reader_new(path);
    if (!zr) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return NULL;
    }

    if (!ora_check_mimetype(zr)) {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_MIMETYPE);
        DP_zip_reader_free(zr);
        return NULL;
    }

    DP_ReadOraContext c = {
        dc,
        zr,
        NULL,
        DP_READ_ORA_EXPECT_IMAGE,
        NULL,
        0,
        0,
        DP_QUEUE_NULL,
        DP_QUEUE_NULL,
    };
    DP_queue_init(&c.groups, 8, sizeof(DP_ReadOraGroup));
    DP_queue_init(&c.children, 8, sizeof(DP_ReadOraChildren));
    DP_CanvasState *cs = ora_read_stack_xml(&c);
    DP_queue_dispose(&c.children);
    DP_queue_dispose(&c.groups);
    DP_zip_reader_free(zr);
    if (cs) {
        return cs;
    }
    else {
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }
}


DP_CanvasState *load_flat_image(DP_DrawContext *dc, DP_Input *input,
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

    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);

    assign_load_result(out_result, DP_LOAD_RESULT_SUCCESS);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_load(DP_DrawContext *dc, const char *path,
                        const char *flat_image_layer_title,
                        DP_LoadResult *out_result)
{
    if (!path) {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        return NULL;
    }

    const char *dot = strrchr(path, '.');
    if (DP_str_equal_lowercase(dot, ".ora")) {
        return load_ora(dc, path, out_result);
    }

    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return NULL;
    }

    DP_CanvasState *cs =
        load_flat_image(dc, input, flat_image_layer_title, out_result);

    DP_input_free(input);
    return cs;
}
