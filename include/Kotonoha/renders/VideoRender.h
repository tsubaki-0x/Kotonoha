#pragma once
#include <Kotonoha/Kotonoha.h>
#include <Kotonoha/utils/FFmpeg.h>
#include <Kotonoha/utils/Time.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

struct Kotonoha_videoData {
  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  struct SwsContext *swsCtx;
  struct ffmpegHwContext *hwCtx;
  int videoStreamIndex;

  AVFrame *pFrame;

  struct Kotonoha_time *time;
  Uint64 startTime, endTime, lastTime, videoTime, frameTime;
  SDL_Texture *texture;

  // KTN-0001-R6: PLAY_MOVIE-only timeline compatibility metadata.
  bool useOrsFrameTimeline;
  Uint64 orsStartFrame;
  Uint64 orsEndFrame;

  // Monotonic decoder instrumentation for the current media lifetime.
  Uint64 decodedOrdinal;
  Uint64 pendingOrdinal;
  Uint64 pendingPtsMs;
  Uint64 uploadedOrdinal;
  Uint64 uploadedPtsMs;
  bool hasPendingFrameMeta;
  bool hasUploadedFrame;
  bool debugTerminalLogged;

  // KTN-0001-DIAG2: diagnostic-only state used to log transitions once.
  int debugLastStatus;
};

struct Kotonoha_videoData *Kotonoha_VideoRenderInit(const char *filename,
                                                    struct Kotonoha_time *time,
                                                    Uint64 startTime,
                                                    Uint64 endTime,
                                                    bool useOrsFrameTimeline,
                                                    Uint64 orsStartFrame,
                                                    Uint64 orsEndFrame);

void Kotonoha_VideoRenderShutdown(struct Kotonoha_videoData **instance);

enum Kotonoha_Scene_Status Kotonoha_VideoRenderProcess(void *userData,
                                                       SDL_Renderer *render);