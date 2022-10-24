#include "save.h"
#include "canvas_state.h"
#include "image.h"
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <ctype.h>
#ifdef DRAWDANCE_OPENRASTER
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

static void warn_zip_error(const char *title, zip_error_t *ze)
{
    DP_warn("Save ORA: %s: %s", title,
            ze ? zip_error_strerror(ze) : "null error");
}

static void warn_zip_archive_error(const char *title, zip_t *archive)
{
    warn_zip_error(title, zip_get_error(archive));
}

static void warn_zip_error_code(const char *title, int errcode)
{
    zip_error_t ze;
    zip_error_init_with_code(&ze, errcode);
    warn_zip_error(title, &ze);
    zip_error_fini(&ze);
}

static bool store_ora_mimetype(zip_t *archive)
{
    static const char *mimetype = "image/openraster";
    zip_source_t *source =
        zip_source_buffer(archive, mimetype, strlen(mimetype), 0);
    if (!source) {
        warn_zip_archive_error("create mimetype source", archive);
        return false;
    }

    zip_int64_t index =
        zip_file_add(archive, "mimetype", source, ZIP_FL_ENC_UTF_8);
    if (index < 0) {
        warn_zip_archive_error("store mimetype", archive);
        zip_source_free(source);
        return false;
    }

    zip_uint64_t uindex = (zip_uint64_t)index;
    if (zip_set_file_compression(archive, uindex, ZIP_CM_STORE, 0) != 0) {
        warn_zip_archive_error("set mimetype compression", archive);
        return false;
    }

    return true;
}

static DP_SaveResult save_ora(DP_CanvasState *cs, const char *path)
{
    int errcode;
    zip_t *archive = zip_open(path, ZIP_CREATE | ZIP_TRUNCATE, &errcode);
    if (!archive) {
        warn_zip_error_code("open archive", errcode);
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    if (!store_ora_mimetype(archive)) {
        zip_discard(archive);
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    // TODO

    if (zip_close(archive) != 0) {
        warn_zip_archive_error("close archive", archive);
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
