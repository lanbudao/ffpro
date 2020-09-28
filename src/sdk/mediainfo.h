#ifndef MEDIAINFO_H
#define MEDIAINFO_H

#include <vector>
#include <string>

typedef struct AudioStreamInfo_ {
    int stream;
    int64_t start_time; /* ms */
    int64_t duration; /* ms */
    int64_t frames;

    /* Codec Paras */
    std::string codec_name;
    int sample_rate;
    int profile;
    int level;
    int format;
    std::string sample_format_s;  /* the sample format, the value corresponds to enum AVSampleFormat. */
    int channels;
    int frame_size;
    uint64_t channel_layout;
	std::string channel_layout_s;

} AudioStreamInfo;

typedef struct VideoStreamInfo_ {
    int stream;
    int64_t start_time; /* ms */
    int64_t duration; /* ms */
    int64_t frames;

    /* Codec Paras */
    std::string codec_name, codec_long;
    int width, height;
    int codec_width, codec_height;
    float rotate = 0.0;
    double pts;
    float aspect_ratio;
    int pixel_format;   /* the pixel format, the value corresponds to enum AVPixelFormat. */
    int delay;
	double frame_rate;
} VideoStreamInfo;

typedef struct MediaInfo_ {
    std::string url;
    int64_t start_time;
    int64_t duration;
    int64_t bit_rate;
    int64_t size;
    int streams;

    std::vector<AudioStreamInfo> audios;
    std::vector<VideoStreamInfo> videos;

    AudioStreamInfo* audio = nullptr;
    VideoStreamInfo* video = nullptr;

} MediaInfo;

#endif // MEDIAINFO_H
