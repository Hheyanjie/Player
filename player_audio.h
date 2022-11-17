#pragma once
#define MAX_AUDIO_FRAME_SIZE 19200
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "SDL.h"
#include "SDL_thread.h"
}

