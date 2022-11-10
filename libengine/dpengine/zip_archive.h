#ifndef DPENGINE_ZIP_ARCHIVE_H
#define DPENGINE_ZIP_ARCHIVE_H
#include <dpcommon/common.h>

typedef struct DP_ZipWriter DP_ZipWriter;


DP_ZipWriter *DP_zip_writer_new(const char *path);

void DP_zip_writer_free_abort(DP_ZipWriter *zw);

bool DP_zip_writer_free_finish(DP_ZipWriter *zw) DP_MUST_CHECK;

bool DP_zip_writer_add_dir(DP_ZipWriter *zw, const char *path) DP_MUST_CHECK;

bool DP_zip_writer_add_file(DP_ZipWriter *zw, const char *path,
                            const void *buffer, size_t size, bool deflate,
                            bool take_buffer) DP_MUST_CHECK;


#endif
