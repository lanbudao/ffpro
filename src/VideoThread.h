#ifndef VIDEOTHREAD_H
#define VIDEOTHREAD_H

#include "AVThread.h"
#include "VideoFrame.h"

NAMESPACE_BEGIN

class VideoThreadPrivate;
class VideoThread: public AVThread
{
    DPTR_DECLARE_PRIVATE(VideoThread)
public:
    VideoThread();
    ~VideoThread();

    void setSubtitleDecoder(AVDecoder* decoder);
    PacketQueue *subtitlePackets();
    void setSubtitlePackets(PacketQueue *packets);

    void pause(bool p) override;

    void stepToNextFrame(std::function<void()> cb);

    void applyFilters(VideoFrame * frame);

protected:
    void run() PU_DECL_OVERRIDE;

protected:
};

NAMESPACE_END
#endif //VIDEOTHREAD_H
