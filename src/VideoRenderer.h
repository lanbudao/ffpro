#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

#include "AVOutput.h"
#include "VideoFrame.h"
#include "renderer/RectF.h"

NAMESPACE_BEGIN

class VideoRendererPrivate;
class VideoRenderer: public AVOutput
{
    DPTR_DECLARE_PRIVATE(VideoRenderer)
public:
//    VideoRenderer(VideoRendererPrivate *d);
    VideoRenderer();

    enum OutAspectRatioMode {
        RendererAspectRatio,
        VideoAspectRatio,
        CustomAspectRation
    };

    virtual ~VideoRenderer();

    void setOpaque(void* o);
    void* opaque() const;

	void initVideoRender();

    virtual void setBackgroundColor(const Color &c);

	void receive(const VideoFrame &frame);

	void setSourceSize(int width, int height);

    void resizeWindow(int width, int height);

    void renderVideo();

	RectF realROI() const;

	void setOutAspectRatio(double ratio);

	void setupAspectRatio();

protected:
    virtual void onResizeWindow();
    virtual void update();

private:
    std::function<void(float)> sourceAspectRatioChanged;

};

NAMESPACE_END
#endif //VIDEORENDERER_H
