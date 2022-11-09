#include "save.h"
#include "canvas_state.h"
#include "image.h"
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <ctype.h>
#ifdef DRAWDANCE_OPENRASTER
#    include "image_png.h"
#    include "layer_content.h"
#    include "layer_group.h"
#    include "layer_list.h"
#    include "layer_props.h"
#    include "layer_props_list.h"
#    include <dpcommon/conversions.h>
#    include <dpcommon/output.h>
#    include <uthash_inc.h>
#    include <zip.h>
#endif


const DP_SaveFormat *DP_save_supported_formats(void)
{
#ifdef DRAWDANCE_OPENRASTER
    static const char *ora_ext[] = {"ora", NULL};
#endif
    static const char *png_ext[] = {"png", NULL};
    static const DP_SaveFormat formats[] = {
#ifdef DRAWDANCE_OPENRASTER
        {"OpenRaster", ora_ext},
#endif
        {"PNG", png_ext},
        {NULL, NULL},
    };
    return formats;
}


#ifdef DRAWDANCE_OPENRASTER

typedef struct DP_SaveOraOffsets {
    int layer_id;
    int offset_x, offset_y;
    UT_hash_handle hh;
} DP_SaveOraOffsets;

typedef struct DP_SaveOraContext {
    zip_t *archive;
    DP_SaveOraOffsets *offsets;
} DP_SaveOraContext;

void save_ora_context_dispose(DP_SaveOraContext *c)
{
    DP_SaveOraOffsets *soo, *tmp;
    HASH_ITER(hh, c->offsets, soo, tmp)
    {
        HASH_DEL(c->offsets, soo);
        DP_free(soo);
    }
}


static const char *ora_strerror(zip_error_t *ze)
{
    if (ze) {
        const char *str = zip_error_strerror(ze);
        if (str) {
            return str;
        }
    }
    return "null error";
}

static const char *ora_archive_error(zip_t *archive)
{
    return ora_strerror(zip_get_error(archive));
}

static bool ora_mkdir(zip_t *archive, const char *name)
{
    if (zip_dir_add(archive, name, ZIP_FL_ENC_UTF_8) != -1) {
        return true;
    }
    else {
        DP_warn("Error creating directory '%s': %s", name,
                ora_archive_error(archive));
        return false;
    }
}

static bool ora_store_buffer(zip_t *archive, const char *name,
                             const void *buffer, size_t size, bool take,
                             zip_int32_t compression)
{
    zip_source_t *source = zip_source_buffer(archive, buffer, size, take);
    if (!source) {
        DP_warn("Error creating source for '%s': %s", name,
                ora_archive_error(archive));
        if (take) {
            DP_free((void *)buffer);
        }
        return false;
    }

    zip_int64_t index = zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8);
    if (index < 0) {
        DP_warn("Error storing '%s': %s", name, ora_archive_error(archive));
        zip_source_free(source);
        return false;
    }

    zip_uint64_t uindex = (zip_uint64_t)index;
    if (zip_set_file_compression(archive, uindex, compression, 0) != 0) {
        DP_warn("Error setting compression for '%s': %s", name,
                ora_archive_error(archive));
        return false;
    }

    return true;
}

static bool ora_store_mimetype(zip_t *archive)
{
    static const char *mimetype = "image/openraster";
    return ora_store_buffer(archive, "mimetype", mimetype, strlen(mimetype),
                            false, ZIP_CM_STORE);
}

static void *image_to_png(DP_Image *img, size_t *out_size)
{
    void **buffer_ptr;
    DP_Output *output = DP_mem_output_new(64, false, &buffer_ptr, &out_size);

    bool ok;
    if (img) {
        ok = DP_image_write_png(img, output);
        DP_image_free(img);
    }
    else {
        ok = DP_image_png_write(output, 0, 0, NULL);
    }

    void *buffer = *buffer_ptr;
    DP_output_free(output);

    if (ok) {
        return buffer;
    }
    else {
        DP_free(buffer);
        return NULL;
    }
}

static bool ora_store_layer(DP_SaveOraContext *c, DP_LayerContent *lc,
                            DP_LayerProps *lp)
{
    DP_SaveOraOffsets *soo = DP_malloc(sizeof(*soo));
    soo->layer_id = DP_layer_props_id(lp);
    soo->offset_x = 0;
    soo->offset_y = 0;
    HASH_ADD_INT(c->offsets, layer_id, soo);

    size_t size;
    void *buffer = image_to_png(
        DP_layer_content_to_image_cropped(lc, &soo->offset_x, &soo->offset_y),
        &size);
    if (!buffer) {
        return false;
    }

    char *name = DP_format("data/layer-%04x.png", soo->layer_id);
    bool ok =
        ora_store_buffer(c->archive, name, buffer, size, true, ZIP_CM_STORE);
    DP_free(name);
    return ok;
}

static bool ora_store_layers(DP_SaveOraContext *c, DP_LayerList *ll,
                             DP_LayerPropsList *lpl)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            DP_LayerList *child_ll = DP_layer_group_children_noinc(lg);
            DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
            if (!ora_store_layers(c, child_ll, child_lpl)) {
                return false;
            }
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            if (!ora_store_layer(c, lc, lp)) {
                return false;
            }
        }
    }
    return true;
}

static DP_SaveResult save_ora(DP_CanvasState *cs, const char *path)
{
    int errcode;
    zip_t *archive = zip_open(path, ZIP_CREATE | ZIP_TRUNCATE, &errcode);
    if (!archive) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, errcode);
        DP_warn("Error opening '%s': %s", path, ora_strerror(&ze));
        zip_error_fini(&ze);
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    if (!ora_store_mimetype(archive) || !ora_mkdir(archive, "data")
        || !ora_mkdir(archive, "Thumbnails")) {
        zip_discard(archive);
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    DP_SaveOraContext c = {archive, NULL};
    ora_store_layers(&c, DP_canvas_state_layers_noinc(cs),
                     DP_canvas_state_layer_props_noinc(cs));
    // TODO

    save_ora_context_dispose(&c);

    if (zip_close(archive) != 0) {
        DP_warn("Error closing '%s': %s", path, ora_archive_error(archive));
        zip_discard(archive);
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    return DP_SAVE_RESULT_SUCCESS;
}

#endif


static DP_SaveResult save_png(DP_Image *img, DP_Output *output)
{
    bool ok = DP_image_write_png(img, output);
    if (ok) {
        return DP_SAVE_RESULT_SUCCESS;
    }
    else {
        DP_warn("Save PNG: %s", DP_error());
        return DP_SAVE_RESULT_WRITE_ERROR;
    }
}


static bool equals_lowercase(const char *a, const char *b)
{
    size_t len = strlen(a);
    if (len == strlen(b)) {
        for (size_t i = 0; i < len; ++i) {
            if (tolower(a[i]) != tolower(b[i])) {
                return false;
            }
        }
        return true;
    }
    else {
        return false;
    }
}

DP_SaveResult DP_save(DP_CanvasState *cs, const char *path)
{
    if (!cs || !path) {
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    const char *dot = strrchr(path, '.');
    if (!dot) {
        return DP_SAVE_RESULT_NO_EXTENSION;
    }
    const char *ext = dot + 1;

#ifdef DRAWDANCE_OPENRASTER
    if (equals_lowercase(ext, "ora")) {
        return save_ora(cs, path);
    }
#endif

    DP_SaveResult (*save_fn)(DP_Image *, DP_Output *);
    if (equals_lowercase(ext, "png")) {
        save_fn = save_png;
    }
    else {
        return DP_SAVE_RESULT_UNKNOWN_FORMAT;
    }

    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_INCLUDE_BACKGROUND | DP_FLAT_IMAGE_INCLUDE_SUBLAYERS);
    if (!img) {
        DP_warn("Save: %s", DP_error());
        return DP_SAVE_RESULT_FLATTEN_ERROR;
    }

    DP_Output *output = DP_file_output_new_from_path(path);
    if (!output) {
        DP_warn("Save: %s", DP_error());
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    DP_SaveResult result = save_fn(img, output);

    DP_output_free(output);
    DP_image_free(img);
    return result;
}
