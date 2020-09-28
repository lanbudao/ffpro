#include "Demuxer.h"
#include "Packet.h"
#include "inner.h"
#include "sdk/mediainfo.h"
#include "AVLog.h"

#include <string>
#include <vector>
#include <map>
#include <mutex>

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/display.h"
#include "libavutil/log.h"
#ifdef __cplusplus
}
#endif

NAMESPACE_BEGIN

class InterruptHandler
{
public:
    InterruptHandler(Demuxer *demuxer):
        mCurAction(None),
        interruptCB(new AVIOInterruptCB()),
        mDemuxer(demuxer)
    {
        interruptCB->callback = handleInterrupt;
        interruptCB->opaque = this;
    }
    enum Action
    {
        None = 0,
        OpenStream,
        FindStream,
        ReadStream
    };
    ~InterruptHandler()
    {
        delete interruptCB;
    }
    AVIOInterruptCB *handler() {return interruptCB;}
    Action action() const {return mCurAction;}
    void begin(Action action)
    {

    }
    void end()
    {

    }
	void setStatus(int s) { status = s; };
    void setInterruptTimeout(int64_t timeout) { mTimeout = timeout;}

    static int handleInterrupt(void *obj)
    {
        InterruptHandler *handler = static_cast<InterruptHandler *>(obj);
        if (!handler)
            return 0;
        switch (handler->action()) {
            case OpenStream:
                break;
            case FindStream:
                break;
            case ReadStream:
                break;
            default:
                break;
        }
        return 1;
    }

private:
	int status;
    int64_t mTimeout;
    Action mCurAction;
    AVIOInterruptCB *interruptCB;
    Demuxer *mDemuxer;
};

class DemuxerPrivate
{
public:
    DemuxerPrivate():
        stream(-1),
        avpkt(nullptr),
        eof(false),
        seekable(false),
        genpts(false),
        has_attached_pic(false),
        seek_type(SeekDefault),
        seek_by_bytes(false),
        realTime(false),
        media_info(nullptr),
        format_ctx(nullptr),
        input_format(nullptr),
        format_opts(nullptr),
        interruptHandler(nullptr),
        show_status(false),
		seek_pos(0)
    {
        memset(stream_index, -1, sizeof(stream_index));
        audio_enabled = video_enabled = subtitle_enabled = true;
#if FFMPEG_MODULE_CHECK(LIBAVFORMAT, 58, 27, 103)
        void *opaque = nullptr;
        av_demuxer_iterate(&opaque);
#else
        av_register_all();
#endif

        avformat_network_init();
        temp = 0;
        avpkt = av_packet_alloc();
    }
    ~DemuxerPrivate()
    {
        if (format_opts) {
            av_dict_free(&format_opts);
            format_opts = nullptr;
        }
        if (format_ctx)
            avformat_free_context(format_ctx);
        if (avpkt)
            av_packet_free(&avpkt);
    }

    bool isRealTime();
    bool isSeekable();

public:
    int stream_index[AVMEDIA_TYPE_NB];
    AVPacket *avpkt;

    int stream;
    bool eof;
    bool seekable;
    bool genpts;
    bool has_attached_pic;
    SeekType seek_type;
    bool seek_by_bytes;
    bool realTime;

    /* A stream specifier can match several streams in the format. */
    const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {nullptr};
    bool audio_enabled, video_enabled, subtitle_enabled;
    Packet curPkt;

    MediaInfo *media_info;

    const char* url;
    AVFormatContext *format_ctx;
    AVInputFormat *input_format;
    AVDictionary *format_opts;
    std::hash<std::string> format_dict;
    InterruptHandler *interruptHandler;
    int temp;
	std::mutex mutex;
    bool show_status;
	double seek_pos;
};

bool DemuxerPrivate::isRealTime()
{
    if (!strcmp(format_ctx->iformat->name, "rtp") ||
        !strcmp(format_ctx->iformat->name, "rtsp") ||
        !strcmp(format_ctx->iformat->name, "sdp") ||
        !strcmp(format_ctx->iformat->name, "hls"/*HTTP Live Streaming, from Apple Inc.*/))
        return true;
    if (format_ctx->pb && (!strncmp(format_ctx->url, "rtp:", 4) || !strncmp(format_ctx->url, "udp:", 4)))
        return true;
    return false;
}

bool DemuxerPrivate::isSeekable()
{
    bool flag = false;
    if (!format_ctx)
        return flag;
    if (format_ctx->pb)
        flag |= !!format_ctx->pb->seekable;
    flag |= (format_ctx->iformat->read_seek || format_ctx->iformat->read_seek2);
    return flag;
}

Demuxer::Demuxer():
    d_ptr(new DemuxerPrivate)
{
    DPTR_D(Demuxer);
    d->interruptHandler = new InterruptHandler(this);
}

Demuxer::~Demuxer()
{

}

void Demuxer::setMedia(const char* url)
{
    DPTR_D(Demuxer);
    d->url = url;
}

void Demuxer::setWantedStreamSpec(MediaType type, const char* spec)
{
    DPTR_D(Demuxer);

    d->wanted_stream_spec[type] = spec;
}

void Demuxer::setMediaStreamDisable(MediaType type)
{
    DPTR_D(Demuxer);

    switch (type) {
    case MediaTypeAudio:
        d->audio_enabled = false;
        break;
    case MediaTypeVideo:
        d->video_enabled = false;
        break;
    case MediaTypeSubtitle:
        d->subtitle_enabled = false;
        break;
    default:
        break;
    }
}

int Demuxer::load()
{
    DPTR_D(Demuxer);
    int ret = 0;
    int i;

    unload();

    d->format_ctx = avformat_alloc_context();
//    d->format_ctx->interrupt_callback = *(d->interruptHandler->handler());
    if (!d->format_ctx) {
        AVError("Could not allocate context.\n");
        return AVERROR(ENOMEM);
    }

    if (avformat_open_input(&d->format_ctx, d->url, d->input_format, &d->format_opts) != 0) {
        avformat_close_input(&d->format_ctx);
        return AVERROR(ENFILE);
    }
    if (d->genpts)
        d->format_ctx->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(d->format_ctx);
    ret = avformat_find_stream_info(d->format_ctx, nullptr);
    if (ret != 0) {
		AVError("Could not find stream in this media.\n");
        return ret;
    }

    if (!d->seek_by_bytes)
        d->seek_by_bytes = !!(d->format_ctx->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", d->format_ctx->iformat->name);

    d->realTime = d->isRealTime();

    if (d->show_status)
        av_dump_format(d->format_ctx, 0, d->url, 0);
    for (i = 0; static_cast<unsigned int>(i) < d->format_ctx->nb_streams; i++) {
        AVStream *st = d->format_ctx->streams[i];
        //Fuck this flag
        st->discard = AVDISCARD_DEFAULT;
        enum AVMediaType type = st->codecpar->codec_type;
        if (type >= 0 && d->wanted_stream_spec[type] && d->stream_index[type] == -1) {
            if (avformat_match_stream_specifier(d->format_ctx, st, d->wanted_stream_spec[type]) > 0) {
                d->stream_index[type] = i;
            }
        }
    }

    for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
        if (d->wanted_stream_spec[i] && d->stream_index[i] == -1) {
            av_log(nullptr, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n",
                   d->wanted_stream_spec[i],
                   av_get_media_type_string(static_cast<AVMediaType>(i)));
            d->stream_index[i] = INT_MAX;
        }
    }

    if (d->audio_enabled) {
        d->stream_index[AVMEDIA_TYPE_AUDIO] =
                av_find_best_stream(d->format_ctx, AVMEDIA_TYPE_AUDIO,
                                    d->stream_index[AVMEDIA_TYPE_AUDIO],
                                    d->stream_index[AVMEDIA_TYPE_VIDEO],
                                    nullptr, 0);
    }
    if (d->video_enabled) {
        d->stream_index[AVMEDIA_TYPE_VIDEO] =
                av_find_best_stream(d->format_ctx, AVMEDIA_TYPE_VIDEO,
                                    d->stream_index[AVMEDIA_TYPE_VIDEO],
                                    -1, nullptr, 0);
    }
    if (d->audio_enabled && d->subtitle_enabled) {
        d->stream_index[AVMEDIA_TYPE_SUBTITLE] =
                av_find_best_stream(d->format_ctx, AVMEDIA_TYPE_SUBTITLE,
                                    d->stream_index[AVMEDIA_TYPE_SUBTITLE],
                                    d->stream_index[AVMEDIA_TYPE_AUDIO] > 0 ?
                                        d->stream_index[AVMEDIA_TYPE_AUDIO] :
                                        d->stream_index[AVMEDIA_TYPE_VIDEO],
                                    nullptr, 0);
    }

    d->seekable = d->isSeekable();

    initMediaInfo();
    return 0;
}

void Demuxer::unload()
{
    DPTR_D(Demuxer);

    if (d->format_ctx) {
        avformat_close_input(&d->format_ctx);
        d->format_ctx = nullptr;
    }
}

bool Demuxer::atEnd()
{
    DPTR_D(const Demuxer);

    if (!d->format_ctx)
        return true;
    return d->eof;
}

void Demuxer::setEOF(bool eof)
{
    DPTR_D(Demuxer);
    d->eof = eof;
}

bool Demuxer::hasAttachedPic() const
{
    DPTR_D(const Demuxer);
    return d->has_attached_pic;
}

bool Demuxer::isRealTime() const
{
    DPTR_D(const Demuxer);
    return d->realTime;
}

bool Demuxer::isSeekable() const
{
    DPTR_D(const Demuxer);
    return d->seekable;
}

bool Demuxer::seek(double pos, double incr)
{
    DPTR_D(Demuxer);

    if (!isSeekable())
        return false;
	if (isnan(pos))
		pos = d->seek_pos;
    pos += incr;
    if (d->seek_type & SeekFromStart)
        incr = 0;
	if (startTime() != AV_NOPTS_VALUE && pos < startTime() / (double)AV_TIME_BASE)
		pos = startTime() / (double)AV_TIME_BASE;
	int64_t seek_target = (int64_t)(pos * AV_TIME_BASE);
    int64_t seek_rel = (int64_t)(incr * AV_TIME_BASE);
    int64_t seek_min = seek_rel > 0 ? seek_target - seek_rel + 2 : INT64_MIN;
    int64_t seek_max = seek_rel < 0 ? seek_target - seek_rel - 2 : INT64_MAX;

	int seek_flags = 0;
	if (d->seek_type & SeekAccurate) {
		seek_flags = AVSEEK_FLAG_ANY;
	}
	else if (d->seek_type & SeekKeyFrame) {
		seek_flags = AVSEEK_FLAG_FRAME;
    }
    int ret = avformat_seek_file(d->format_ctx, -1, seek_min, seek_target, seek_max, seek_flags);
    /* need to add AVSEEK_FLAG_BACKWARD flag if use av_seek_frame */
    //if (incr < 0)
    //    seek_flags |= AVSEEK_FLAG_BACKWARD;
    //int ret = av_seek_frame(d->format_ctx, -1, seek_target, seek_flags);
    if (ret < 0) {
        static char str[AV_ERROR_MAX_STRING_SIZE];
        memset(str, 0, sizeof(str));
        return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, ret);
        AVError("seek error: %s\n", str);
        return false;
    }
	d->seek_pos = pos;
    return true;
}

void Demuxer::setSeekType(SeekType type)
{
    DPTR_D(Demuxer);
    d->seek_type = type;
}

void Demuxer::setInterruptStatus(int interrupt)
{
	DPTR_D(Demuxer);
	d->interruptHandler->setStatus(interrupt);
}

int Demuxer::readFrame()
{
    DPTR_D(Demuxer);

	std::lock_guard<std::mutex> lock(d->mutex);
    int ret = -1;
    AVPacket *avpkt = d->avpkt;

    d->interruptHandler->begin(InterruptHandler::ReadStream);
    ret = av_read_frame(d->format_ctx, avpkt);
    d->interruptHandler->end();

    if (ret < 0) {
        if (ret == AVERROR_EOF)
            d->eof = true;
        av_packet_unref(avpkt);
        return ret;
    }
    d->stream = avpkt->stream_index;
    if (d->stream != d->stream_index[MediaTypeAudio] &&
            d->stream != d->stream_index[MediaTypeVideo] &&
            d->stream != d->stream_index[MediaTypeSubtitle]) {
        av_packet_unref(avpkt);
        return -1;
    }

    d->curPkt = Packet::fromAVPacket(avpkt, av_q2d(d->format_ctx->streams[d->stream]->time_base));
    av_packet_unref(avpkt);
    d->eof = false;

    return ret;
}

AVFormatContext *Demuxer::formatCtx() const
{
    return d_func()->format_ctx;
}

void Demuxer::setMediaInfo(MediaInfo *info)
{
    DPTR_D(Demuxer);
    d->media_info = info;
}

void Demuxer::initMediaInfo()
{
    DPTR_D(Demuxer);
    unsigned int i;

    d->media_info->url = d->url;
    d->media_info->bit_rate = d->format_ctx->bit_rate;
    d->media_info->start_time = d->format_ctx->start_time / AV_TIME_BASE;
    d->media_info->duration = d->format_ctx->duration / AV_TIME_BASE;

    for (i = 0; FORCE_UINT(i) < d->format_ctx->nb_streams; i++) {
        AVStream *st = d->format_ctx->streams[i];
        d->has_attached_pic = st->disposition & AV_DISPOSITION_ATTACHED_PIC;
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AudioStreamInfo info;
            info.stream = i;
            info.start_time = st->start_time == AV_NOPTS_VALUE ?
                        0 : FORCE_INT64(st->start_time * av_q2d(st->time_base) * 1000);
            info.duration = st->duration = AV_NOPTS_VALUE ?
                        0 : FORCE_INT64(st->duration * av_q2d(st->time_base) * 1000);
            info.frames = st->nb_frames;
            /* Codec Paras */
//            info.codec_name = st->codec->codec_descriptor->name;
            info.profile = st->codecpar->profile;
            info.level = st->codecpar->level;
            info.sample_rate = st->codecpar->sample_rate;
            info.format = st->codecpar->format;
            info.sample_format_s = av_get_sample_fmt_name(static_cast<AVSampleFormat>(st->codecpar->format));
            info.channels = st->codecpar->channels;
            info.frame_size = st->codecpar->frame_size;
            info.channel_layout = st->codecpar->channel_layout;
            char buf[128];
            av_get_channel_layout_string(buf, sizeof(buf), info.channels, info.channel_layout);
            info.channel_layout_s = std::string(buf);
            d->media_info->audios.push_back(info);
            if (d->stream_index[MediaTypeAudio] == i) {
                d->media_info->audio = &d->media_info->audios[d->media_info->audios.size() - 1];
            }
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            VideoStreamInfo info;
            info.stream = i;
            info.start_time = st->start_time == AV_NOPTS_VALUE ?
                        0 : FORCE_INT64(st->start_time * av_q2d(st->time_base) * 1000);
            info.duration = st->duration = AV_NOPTS_VALUE ?
                        0 : FORCE_INT64(st->duration * av_q2d(st->time_base) * 1000);
            info.frames = st->nb_frames;
            /* Codec Paras */
            info.codec_name = avcodec_get_name(st->codecpar->codec_id);
            info.width = st->codecpar->width;
            info.height = st->codecpar->height;
            info.pixel_format = st->codecpar->format;
            info.delay = st->codecpar->video_delay;
            info.rotate = 0.0;
#if AV_MODULE_CHECK(LIBAVFORMAT, 55, 18, 0, 39, 100)
            AVRational rat = st->display_aspect_ratio;
            info.aspect_ratio = rat.den == 0 ? 0 : FORCE_FLOAT(rat.num) / FORCE_FLOAT(rat.den);
            uint8_t *sd = av_stream_get_side_data(st, AV_PKT_DATA_DISPLAYMATRIX, nullptr);
            if (sd) {
                double r = av_display_rotation_get((int32_t*)(sd));
                if (!isnan(r))
                    info.rotate = (FORCE_INT(r) + 360) % 360;
            }
#endif
			if (st->avg_frame_rate.den && st->avg_frame_rate.num)
				info.frame_rate = av_q2d(st->avg_frame_rate);
#if (defined FF_API_R_FRAME_RATE && FF_API_R_FRAME_RATE)
			//avg_frame_rate may be nan, r_frame_rate may be wrong(guessed value)
			else if (st->r_frame_rate.den && st->r_frame_rate.num) {
				if (st->r_frame_rate.num < 90000)
					info.frame_rate = av_q2d(st->r_frame_rate);
				AVDebug("r_frame_rate-->%d/%d\n", st->r_frame_rate.num, st->r_frame_rate.den);
			}
#endif
            d->media_info->videos.push_back(info);
            if (d->stream_index[MediaTypeVideo] == i) {
                d->media_info->video = &d->media_info->videos[d->media_info->videos.size() - 1];
            }
        }
    }
}

int64_t Demuxer::startTime()
{
    DPTR_D(Demuxer);
    if (d->format_ctx && d->format_ctx->start_time != AV_NOPTS_VALUE) {
        return d->format_ctx->start_time;
    }
    return 0;
}

int64_t Demuxer::duration()
{
    DPTR_D(Demuxer);
    if (d->format_ctx && d->format_ctx->duration != AV_NOPTS_VALUE) {
        return d->format_ctx->duration / AV_TIME_BASE;
    }
    return 0;
}

int Demuxer::stream() const
{
    DPTR_D(const Demuxer);
    return d->stream;
}

int Demuxer::streamIndex(MediaType type) const
{
    DPTR_D(const Demuxer);
    return d->stream_index[type];
}

AVStream* Demuxer::stream(MediaType type) const
{
    DPTR_D(const Demuxer);
    if (d->format_ctx && d->stream_index[type] >= 0)
        return d->format_ctx->streams[d->stream_index[type]];
    return nullptr;
}

const Packet &Demuxer::packet() const
{
    return d_func()->curPkt;
}

double Demuxer::maxDuration() const
{
    DPTR_D(const Demuxer);

    if (d->format_ctx && d->format_ctx->iformat)
        return (d->format_ctx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    return 0;
}

NAMESPACE_END