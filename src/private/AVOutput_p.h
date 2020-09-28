#ifndef AVOUTPUT_P_H
#define AVOUTPUT_P_H

#include "sdk/DPTR.h"
#include "Size.h"
#include "sdk/mediainfo.h"
#include "VideoFrame.h"
#include <shared_mutex>

NAMESPACE_BEGIN

class AVOutputPrivate
{
public:
    AVOutputPrivate():
        avaliable(true),
        pause(false),
//        statistics(nullptr)
        media_info(nullptr)
    {

    }
    virtual ~AVOutputPrivate()
    {

    }

    bool avaliable;
    bool pause;
    MediaInfo *media_info;
    VideoFrame current_frame;
    //Callback
    std::function<void(void* vo_opaque)> render_cb = nullptr;
    std::mutex mtx;
};

NAMESPACE_END
#endif //AVOUTPUT_P_H