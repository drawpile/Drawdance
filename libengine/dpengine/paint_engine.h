#ifndef DPENGINE_PAINT_ENGINE
#define DPENGINE_PAINT_ENGINE
#include "canvas_diff.h"
#include "canvas_history.h"
#include <dpcommon/common.h>

typedef struct DP_AclState DP_AclState;
typedef struct DP_AnnotationList DP_AnnotationList;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DocumentMetadata DP_DocumentMetadata;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_Message DP_Message;
typedef union DP_Pixel8 DP_Pixel8;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
#else
typedef struct DP_LayerContent DP_TransientLayerContent;
#endif

typedef void (*DP_PaintEngineAclsChangedFn)(void *user, int acl_change_flags);
typedef void (*DP_PaintEngineLaserTrailFn)(void *user, unsigned int context_id,
                                           int persistence, uint32_t color);
typedef void (*DP_PaintEngineMovePointerFn)(void *user, unsigned int context_id,
                                            int x, int y);
typedef void (*DP_PaintEngineDefaultLayerSetFn)(void *user, int layer_id);
typedef void (*DP_PaintEngineCatchupFn)(void *user, int progress);
typedef void (*DP_PaintEngineResizedFn)(void *user, int offset_x, int offset_y,
                                        int prev_width, int prev_height);
typedef void (*DP_PaintEngineLayerPropsChangedFn)(void *user,
                                                  DP_LayerPropsList *lpl);
typedef void (*DP_PaintEngineAnnotationsChangedFn)(void *user,
                                                   DP_AnnotationList *al);
typedef void (*DP_PaintEngineDocumentMetadataChangedFn)(
    void *user, DP_DocumentMetadata *dm);
typedef void (*DP_PaintEngineCursorMovedFn)(void *user, unsigned int context_id,
                                            int layer_id, int x, int y);
typedef void (*DP_PaintEngineRenderSizeFn)(void *user, int width, int height);
typedef void (*DP_PaintEngineRenderTileFn)(void *user, int x, int y,
                                           DP_Pixel8 *pixels);


typedef struct DP_PaintEngine DP_PaintEngine;

DP_PaintEngine *
DP_paint_engine_new_inc(DP_AclState *acls, DP_CanvasState *cs_or_null,
                        DP_CanvasHistorySavePointFn save_point_fn,
                        void *save_point_user);

void DP_paint_engine_free_join(DP_PaintEngine *pe);

DP_TransientLayerContent *
DP_paint_engine_render_content_noinc(DP_PaintEngine *pe);

void DP_paint_engine_local_drawing_in_progress_set(
    DP_PaintEngine *pe, bool local_drawing_in_progress);

void DP_paint_engine_layer_visibility_set(DP_PaintEngine *pe, int layer_id,
                                          bool hidden);

// Returns the number of drawing commands actually pushed to the paint engine.
int DP_paint_engine_handle_inc(
    DP_PaintEngine *pe, bool local, int count, DP_Message **msgs,
    DP_PaintEngineAclsChangedFn acls_changed,
    DP_PaintEngineLaserTrailFn laser_trail,
    DP_PaintEngineMovePointerFn move_pointer,
    DP_PaintEngineDefaultLayerSetFn default_layer_set, void *user);

void DP_paint_engine_tick(
    DP_PaintEngine *pe, DP_PaintEngineCatchupFn catchup,
    DP_PaintEngineResizedFn resized, DP_CanvasDiffEachPosFn tile_changed,
    DP_PaintEngineLayerPropsChangedFn layer_props_changed,
    DP_PaintEngineAnnotationsChangedFn annotations_changed,
    DP_PaintEngineDocumentMetadataChangedFn document_metadata_changed,
    DP_PaintEngineCursorMovedFn cursor_moved, void *user);

void DP_paint_engine_prepare_render(DP_PaintEngine *pe,
                                    DP_PaintEngineRenderSizeFn render_size,
                                    void *user);

void DP_paint_engine_render(DP_PaintEngine *pe,
                            DP_PaintEngineRenderTileFn render_tile, void *user);

void DP_paint_engine_preview_cut(DP_PaintEngine *pe, int layer_id, int x, int y,
                                 int width, int height,
                                 const DP_Pixel8 *mask_or_null);

void DP_paint_engine_preview_dabs_inc(DP_PaintEngine *pe, int layer_id,
                                      int count, DP_Message **messages);

void DP_paint_engine_preview_clear(DP_PaintEngine *pe);

DP_CanvasState *DP_paint_engine_canvas_state_inc(DP_PaintEngine *pe);


#endif
