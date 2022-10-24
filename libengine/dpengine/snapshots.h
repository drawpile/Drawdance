#ifndef DPENGINE_SNAPSHOTS_H
#define DPENGINE_SNAPSHOTS_H
#include <dpcommon/common.h>

typedef struct DP_CanvasHistory DP_CanvasHistory;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Message DP_Message;


typedef struct DP_Snapshot DP_Snapshot;
typedef struct DP_SnapshotQueue DP_SnapshotQueue;

typedef long long (*DP_SnapshotQueueTimestampMsFn)(void *user);

typedef DP_Snapshot *(*DP_SnapshotAtFn)(DP_SnapshotQueue *sq, size_t index);

typedef void (*DP_SnapshotsGetFn)(void *user, DP_SnapshotQueue *sq,
                                  size_t count, DP_SnapshotAtFn at);


long long DP_snapshot_timestamp_ms(DP_Snapshot *s);

DP_CanvasState *DP_snapshot_canvas_state_noinc(DP_Snapshot *s);


DP_SnapshotQueue *
DP_snapshot_queue_new(size_t max_count, long long min_delay_ms,
                      DP_SnapshotQueueTimestampMsFn timestamp_fn,
                      void *timestamp_user);

void DP_snapshot_queue_free(DP_SnapshotQueue *sq);

// Pass this to canvas_history_new to wire up the snapshot queue.
void DP_snapshot_queue_on_save_point(void *user, DP_CanvasState *cs,
                                     bool snapshot_requested);

void DP_snapshot_queue_get_with(DP_SnapshotQueue *sq, DP_SnapshotsGetFn get_fn,
                                void *user);


void DP_reset_image_build(DP_CanvasState *cs, unsigned int context_id,
                          void (*push_message)(void *, DP_Message *),
                          void *user);


#endif
