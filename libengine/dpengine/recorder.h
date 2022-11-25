#ifndef DPMSG_RECORDER_H
#define DPMSG_RECORDER_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


typedef struct DP_Recorder DP_Recorder;

typedef enum DP_RecorderType {
    DP_RECORDER_TYPE_BINARY,
    DP_RECORDER_TYPE_TEXT,
} DP_RecorderType;

typedef long long (*DP_RecorderGetTimeMsFn)(void *user);


DP_Recorder *DP_recorder_new_inc(DP_RecorderType type, DP_CanvasState *cs,
                                 DP_RecorderGetTimeMsFn get_time_fn,
                                 void *get_time_user, DP_Output *output);

void DP_recorder_free_join(DP_Recorder *r);

void DP_recorder_message_push_initial_inc(DP_Recorder *r, int count,
                                          DP_Message *(*get)(void *, int),
                                          void *user);

bool DP_recorder_message_push_inc(DP_Recorder *r, DP_Message *msg);


#endif
