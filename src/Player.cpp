#include "player_p.h"
#include "glad/glad.h"

NAMESPACE_BEGIN

Player::Player():
    d_ptr(new PlayerPrivate)
{
}

Player::~Player()
{
	stop();
}

//void Player::loadGLLoader(std::function<void*(const char*)> p)
//{
    //gladLoadGL();
	//gladLoadGLLoader((GLADloadproc)(*p.target<void*(*)(const char*)>()));
//}

void Player::setMedia(const std::string& url)
{
    DPTR_D(Player);

    d->url = url;
    d->demuxer->setMedia(url);
}

void Player::setWantedStreamSpec(MediaType type, const char* spec)
{
    DPTR_D(Player);

    d->demuxer->setWantedStreamSpec(type, spec);
}

int Player::mediaStreamIndex(MediaType type)
{
    return d_func()->demuxer->streamIndex(type);
}

void Player::setMediaStreamDisable(MediaType type)
{
    DPTR_D(Player);

    d->demuxer->setMediaStreamDisable(type);
}

void Player::prepare()
{
    DPTR_D(Player);

    if (d->url.empty()) {
        AVWarning("media url is empty!\n");
        return;
    }
    stop();
    d->media_status = Loading;
    CALL_BACK(d->mediaStatusChanged, Loading);
    d->loaded = !(d->demuxer->load());
    if (!d->loaded) {
        printf("Load media async failed.\n");
        d->media_status = Invalid;
        CALL_BACK(d->mediaStatusChanged, Invalid);
        return;
    }
    d->media_status = Loaded;
    CALL_BACK(d->mediaStatusChanged, Loaded);
    d->initRenderVideo();
    d->clock.setMaxDuration(d->demuxer->maxDuration());
    d->applySubtitleStream();
    d->playInternal();
    d->media_status = Prepared;
    CALL_BACK(d->mediaStatusChanged, Prepared);
}

void Player::prepareAsync()
{
    DPTR_D(Player);
    if (d->url.empty()) {
        AVWarning("media url is empty!\n");
        return;
    }
    stop();
    d->load_media_async = new LoadMediaAsync(this, d);
    d->load_media_async->start();
}

MediaStatus Player::mediaStatus()
{
    return d_func()->media_status;
}

void Player::pause(bool p)
{
    DPTR_D(Player);
    if (!d->loaded)
        return;
    if (d->paused == p)
        return;
    d->paused = p;
    if (d->ao) d->ao->pause(p);
    d->demux_thread->pause(p);
	d->clock.pause(p);
}

bool Player::isPaused() const
{
    return d_func()->paused;
}

void Player::stop()
{
	DPTR_D(Player);

    if (d->load_media_async) {
        d->load_media_async->stop();
        d->load_media_async->wait();
        delete d->load_media_async;
        d->load_media_async = nullptr;
    }
    if (d->media_status == Unloaded)
        return;
	//if (!d->loaded)
	//	return;
	d->demux_thread->stop();
    d->demux_thread->wait();
    if (d->audio_thread) {
        d->audio_thread->packets()->clear();
        d->audio_thread->packets()->blockFull(false);
        d->audio_thread->stop();
        d->audio_thread->wait();
        delete d->audio_thread;
        d->audio_thread = nullptr;
    }
    if (d->video_thread) {
        d->video_thread->packets()->clear();
        d->video_thread->packets()->blockFull(false);
        d->video_thread->stop();
        d->video_thread->wait();
        delete d->video_thread;
        d->video_thread = nullptr;
    }
	d->demuxer->unload();
    d->loaded = false;
    d->media_status = Unloaded;
    CALL_BACK(d->mediaStatusChanged, Unloaded);
}

void Player::setSpeed(float speed)
{
    DPTR_D(Player);
    d->clock.setSpeed(speed);
}

float Player::speed() const
{
    DPTR_D(const Player);
    return d->clock.speed();
}

bool Player::isMute() const
{
    return d_func()->ao->isMute();
}

void Player::setMute(bool m)
{
    d_func()->ao->setMute(m);
}

void Player::setVolume(float volume)
{
    d_func()->ao->setVolume(volume);
}

float Player::volume() const
{
    return d_func()->ao->volume();
}

bool Player::isPlaying()
{
    DPTR_D(Player);
    return (d->demux_thread && d->demux_thread->isRunning()) ||
            (d->audio_thread && d->audio_thread->isRunning()) ||
            (d->video_thread && d->video_thread->isRunning());
}

void Player::seek(double t, SeekType type)
{
    DPTR_D(Player);

    AVDebug("seek started.\n");
    if (d->media_status != Prepared) {
        AVDebug("media is not prepared.\n");
        return;
    }
    if (!d->demuxer->isSeekable())
        return;
    /* if pos is nan, it indicates that the previous seek operation has not been completed */
    double pos = position();
    if (isnan(pos)) {
        AVDebug("pts is nan, seek isn't finished.\n");
        return;
    }
    if (d->clock.type() != SyncToVideo 
        && isnan(d->clock.value(SyncToVideo))
        && !d->demuxer->hasAttachedPic()) /* don't care video clock when audio has attached pic*/
    {
        AVDebug("Video seek is not finished.\n");
        return;
    }
    double seek_pos = 0;
    bool forward = true;
    if (type & SeekFromStart) {
        seek_pos += d->demuxer->startTimeS();
        forward = t > pos;
    }
    else {
        seek_pos += pos;
        forward = t > 0;
    }
    AVDebug("Seek to %.3f, %.3f.\n", pos, seek_pos);
    d->demux_thread->seek(seek_pos, t, type);
    CALL_BACK(d->seekRequest, seek_pos, t, type);
}

void Player::seekForward(int incr)
{
    this->seek(incr, SeekType(SeekFromNow | SeekKeyFrame));
}

void Player::seekBackward(int incr)
{
    this->seek(incr, SeekType(SeekFromNow | SeekKeyFrame));
}

void Player::setBufferPara(BufferMode mode, int64_t value)
{
    DPTR_D(Player);
    d->buffer_mode = mode;
    d->buffer_value = value;
}

MediaInfo* Player::info()
{
    DPTR_D(Player);
    return &d->mediainfo;
}

double Player::position()
{
    DPTR_D(Player);
    return d->clock.value();
}

int64_t Player::duration()
{
    DPTR_D(Player);
    return d->demuxer->duration();
}

void Player::setClockType(ClockType type)
{
    DPTR_D(Player);
    d->clock_type = type;
}

void Player::setResampleType(ResampleType t)
{
    DPTR_D(Player);
    d->resample_type = t;
}

void Player::renderVideo()
{
    DPTR_D(Player);
    for (AVOutput* output: d->video_output_set.outputs()) {
		VideoRenderer* render = static_cast<VideoRenderer*>(output);
        render->renderVideo();
    }
}

void Player::setRenderCallback(std::function<void (void *)> cb)
{
    DPTR_D(Player);
    for (AVOutput* output: d->video_output_set.outputs()) {
        output->setRenderCallback(cb);
    }
}

VideoRenderer* Player::setVideoRenderer(int w, int h, void* opaque)
{
    DPTR_D(Player);
    std::list<AVOutput*> outputs = d->video_output_set.outputs();
    std::list<AVOutput *>::iterator itor;
    for (itor = outputs.begin(); itor != outputs.end(); itor++) {
        VideoRenderer *render = reinterpret_cast<VideoRenderer *>(*itor);
        if (render->opaque() == opaque) {
            render->resizeWindow(w, h);
        }
    }
    VideoRenderer *renderer = new VideoRenderer();
    renderer->setOpaque(opaque);
    renderer->setMediaInfo(&d->mediainfo); // for test
    renderer->resizeWindow(w, h);
    d->video_output_set.addOutput((AVOutput*)renderer);
    d->renderToFilters.insert(std::make_pair(renderer, std::list<Filter*>()));
    return renderer;
}

void Player::addVideoRenderer(VideoRenderer *renderer)
{
    DPTR_D(Player);
    renderer->setMediaInfo(&d->mediainfo);
    d->video_output_set.addOutput((AVOutput*)renderer);
    d->renderToFilters.insert(std::make_pair(renderer, std::list<Filter*>()));
}

void Player::removeRenderer(VideoRenderer * renderer)
{
    //TODO
}

void Player::resizeWindow(int w, int h)
{
    DPTR_D(Player);
    for (AVOutput* output: d->video_output_set.outputs()) {
        VideoRenderer* render = static_cast<VideoRenderer*>(output);
        render->resizeWindow(w, h);
    }
}

bool Player::installFilter(AudioFilter *filter, int index)
{
    DPTR_D(Player);
    filter->setPlayer(this);
    return d->installFilter(filter, index);
}

bool Player::installFilter(VideoFilter * filter, VideoRenderer * render, int index)
{
    DPTR_D(Player);
    filter->setPlayer(this);
    return d->installFilter(filter, render, index);
}

bool Player::installFilter(RenderFilter * filter, VideoRenderer * render, int index)
{
    DPTR_D(Player);
    filter->setPlayer(this);
    return d->installFilter(filter, render, index);
}

void Player::enableExternalSubtitle(Subtitle *sub)
{
    DPTR_D(Player);
    std::list<Subtitle*>::iterator itor = d->external_subtitles.begin();
    for (; itor != d->external_subtitles.end(); itor++) {
        (*itor)->setEnabled(sub == (*itor));
    }
}

void Player::setInternalSubtitleEnabled(bool enabled, VideoRenderer * r)
{
    DPTR_D(Player);

    std::list<AVOutput*> outputs = d->video_output_set.outputs();
    std::list<AVOutput *>::iterator itor;
    for (itor = outputs.begin(); itor != outputs.end(); itor++) {
        VideoRenderer *r = reinterpret_cast<VideoRenderer *>(*itor);
        r->setInternalSubtitleEnabled(enabled);
    }
}

Subtitle *Player::addExternalSubtitle(const string &url, bool enabled, VideoRenderer* render)
{
    DPTR_D(Player);
    Subtitle *sub = new Subtitle;
    sub->setEnabled(enabled);
    sub->setFile(url);
    d->external_subtitles.push_back(sub);
    sub->load();

    /* add to video render */
    std::list<AVOutput*> outputs = d->video_output_set.outputs();
    std::list<AVOutput *>::iterator itor;
    for (itor = outputs.begin(); itor != outputs.end(); itor++) {
        VideoRenderer *r = reinterpret_cast<VideoRenderer *>(*itor);
        if (r == render || render == nullptr) {
            r->addSubtitle(sub);
        }
    }
    return sub;
}

void Player::removeExternalSubtitle(Subtitle *sub)
{
    DPTR_D(Player);
    std::list<Subtitle*>::iterator itor = d->external_subtitles.begin();
    for (; itor != d->external_subtitles.end(); itor++) {
        if (sub == nullptr || sub == (*itor)) {
            delete (*itor);
            d->external_subtitles.remove(*itor);
            break;
        }
    }
}

std::list<Subtitle *> Player::externalSubtitles()
{
    DPTR_D(Player);
    return d->external_subtitles;
}

void Player::setBufferProcessCallback(std::function<void(float p)> f)
{
    DPTR_D(Player);
    d->demux_thread->setBufferProcessChangedCB(f);
}

void Player::setMediaStatusCallback(std::function<void (MediaStatus)> f)
{
    DPTR_D(Player);
    d->mediaStatusChanged = f;
}

void Player::setStreamChangedCallback(std::function<void(MediaType type, int stream)> f)
{
    DPTR_D(Player);
    d->mediaStreamChanged = f;
}

void Player::setSubtitlePacketCallback(std::function<void(Packet* pkt)> f)
{
    DPTR_D(Player);
    d->demux_thread->setSubtitlePacketCallback(f);
}

void Player::setSubtitleHeaderCallback(std::function<void(MediaInfo*)> f)
{
    DPTR_D(Player);
    d->subtitleHeaderChanged = f;
    d->applySubtitleStream();
}

void Player::setSeekRequestCallback(std::function<void(double pos, double incr, SeekType type)> f)
{
    DPTR_D(Player);
    d->seekRequest = f;
}

NAMESPACE_END
