#include "AudioVideoThread.h"
#include <QDebug>

#define __STDC_CONSTANT_MACROS
#define SDL_MAIN_HANDLED
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
};

//Buffer:
//static int audio_len0;
//static Uint8 *audio_pos0;
//static int audio_len1;
//static Uint8 *audio_pos1;

//Refresh Event
#define AV_SFM_REFRESH_EVENT  (SDL_USEREVENT + 5)
#define AV_SFM_BREAK_EVENT  (SDL_USEREVENT + 6)
#define FPS_KEEP 300
#define AUDIO_BUFFER_COUNT 20

static int av_refresh_thread_exit = 0;
static int av_refresh_thread_pause = 0;
static int audio_callback_idx = -1;

static int valid_video_frame_count = 0;
static int video_frame_count = 0;
static int audio_frame_count = 0;

class AudioPlayBuffer
{
public:
    int audio_len;
    Uint8 *audio_pos;
    uint8_t *out_buffer_audio;

    AudioPlayBuffer() : audio_len(8820), audio_pos(0)
    {
        out_buffer_audio = (uint8_t *)av_malloc(8820*2);
    }

    ~AudioPlayBuffer()
    {
        av_free(out_buffer_audio);
    }
};

static AudioPlayBuffer *apbArray[AUDIO_BUFFER_COUNT];

int avRefreshThread(void *)
{
    while(!av_refresh_thread_exit)
    {
        //if(!av_refresh_thread_pause)
        {
            SDL_Event event;
            event.type = AV_SFM_REFRESH_EVENT;
            SDL_PushEvent(&event);
        }
        SDL_Delay(1);
    }

    SDL_Event event;
    event.type = AV_SFM_BREAK_EVENT;
    SDL_PushEvent(&event);

    return 0;
}

void avAudioCallBack(void *, Uint8 *stream, int len)
{
    SDL_memset(stream, 0, len);

    if(av_refresh_thread_exit)
    {
        return;
    }
    if(av_refresh_thread_pause)
    {
        return;
    }

    while((audio_frame_count < 1) && !av_refresh_thread_exit && !av_refresh_thread_pause)
    {
        SDL_Delay(1);
    }
    if(audio_callback_idx < 0)
    {
        audio_callback_idx = audio_frame_count - 1;
    }
    if((audio_frame_count - audio_callback_idx) > AUDIO_BUFFER_COUNT)
    {
        audio_callback_idx = audio_frame_count - 1;
    }
    int storage_idx = audio_callback_idx % AUDIO_BUFFER_COUNT;
    AudioPlayBuffer *apb = apbArray[storage_idx];
    if(apb->audio_len == 0)
    {
        return;
    }
    if(apb->audio_pos == nullptr)
    {
        return;
    }

    if(len > apb->audio_len)
    {
        len = apb->audio_len;
    }
    else
    {
    }
    SDL_MixAudio(stream, apb->audio_pos, len, SDL_MIX_MAXVOLUME);
    ++audio_callback_idx;
}

AudioVideoThread::AudioVideoThread(QString video_device_name, QString audio_device_name, int config_width, int config_height, int config_fps, int isPause)
{
    audio_callback_idx = -1;
    av_refresh_thread_exit = 0;
    valid_video_frame_count = 0;
    video_frame_count = 0;
    audio_frame_count = 0;
    av_refresh_thread_pause = isPause;
    device_name = "video=" + video_device_name + ":audio=" + audio_device_name;
    str_video_size = QString::number(config_width) + "x" + QString::number(config_height);
    str_fps = QString::number(config_fps);

    // init apbArray
    for(int i = 0; i < AUDIO_BUFFER_COUNT; ++i)
    {
        apbArray[i] = new AudioPlayBuffer();
    }
}

AudioVideoThread::~AudioVideoThread()
{
    qDebug("AudioVideoThread destructor");
    for(int i = 0; i < AUDIO_BUFFER_COUNT; ++i)
    {
        delete apbArray[i];
    }
    //av_free(out_buffer_audio0);
    //av_free(out_buffer_audio1);
    av_free(out_buffer_video0);
    av_free(out_buffer_video1);
    av_free(out_buffer_video2);
}

void AudioVideoThread::run()
{
    char cameraname[256];
    strcpy_s(cameraname, device_name.toStdString().c_str());
    char char_video_size[128];
    strcpy_s(char_video_size, str_video_size.toStdString().c_str());
    char char_fps[64];
    strcpy_s(char_fps, str_fps.toStdString().c_str());

    AVDictionary *av_option = 0;
    //av_dict_set(&av_option, "rtbufsize", "", NULL);
    av_dict_set(&av_option, "vcodec", "mjpeg", NULL);
    av_dict_set(&av_option, "video_size", char_video_size, NULL);
    av_dict_set(&av_option, "framerate", char_fps, NULL);
    av_dict_set(&av_option, "audio_buffer_size", "50", NULL);

    AVFormatContext	*pFormatCtx = avformat_alloc_context();
    AVInputFormat *iformat = av_find_input_format("dshow");
    if(avformat_open_input(&pFormatCtx,cameraname,iformat,&av_option)!=0)
    {
        qDebug("Couldn't open input stream.\n");
        return;
    }

    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        qDebug("Couldn't find stream information.\n");
        return;
    }

    int audioindex = -1;
    int videoindex = -1;
    for(unsigned i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoindex < 0)
        {
            videoindex = i;
        }
        if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioindex < 0)
        {
            audioindex = i;
        }
    }

    if(audioindex < 0 || videoindex < 0)
    {
        qDebug("Didn't find a audio or video stream.\n");
        return;
    }

    AVCodecContext *pCodecCtx_Audio = avcodec_alloc_context3(NULL);
    AVCodecContext *pCodecCtx_Video = avcodec_alloc_context3(NULL);
    if (!pCodecCtx_Audio || !pCodecCtx_Video)
    {
        qDebug("Could not allocate AVCodecContext\n");
        return;
    }
    avcodec_parameters_to_context(pCodecCtx_Audio, pFormatCtx->streams[audioindex]->codecpar);
    avcodec_parameters_to_context(pCodecCtx_Video, pFormatCtx->streams[videoindex]->codecpar);

    AVCodec *pCodec_Audio = avcodec_find_decoder(pCodecCtx_Audio->codec_id);
    AVCodec *pCodec_Video = avcodec_find_decoder(pCodecCtx_Video->codec_id);
    if(!pCodec_Audio || !pCodec_Video)
    {
        qDebug("Codec not found.\n");
        return;
    }

    if(avcodec_open2(pCodecCtx_Audio, pCodec_Audio, NULL) < 0 || avcodec_open2(pCodecCtx_Video, pCodec_Video, NULL) < 0)
    {
        qDebug("Could not open codec.\n");
        return;
    }

    //=====================================
    //Audio Stream
    //=====================================
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    int out_nb_samples = 2205;//pCodecCtx_Audio->frame_size;
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
    int out_buffer_size = 8820;
    //int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 0);
    //out_buffer_audio0 = (uint8_t *)av_malloc(out_buffer_size*2);
    //out_buffer_audio1 = (uint8_t *)av_malloc(out_buffer_size*2);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        qDebug( "Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = out_nb_samples;
    wanted_spec.callback = avAudioCallBack;
    wanted_spec.userdata = pCodecCtx_Audio;

    if(SDL_OpenAudio(&wanted_spec, NULL) < 0)
    {
        qDebug("can't open audio.\n");
        return;
    }

    int64_t in_channel_layout = av_get_default_channel_layout(pCodecCtx_Audio->channels);
    struct SwrContext *au_convert_ctx = swr_alloc();
    au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
                                        in_channel_layout, pCodecCtx_Audio->sample_fmt, pCodecCtx_Audio->sample_rate, 0, NULL);

//    qDebug("out_channel_layout = %lld", out_channel_layout);
//    qDebug("out_sample_fmt = %d", out_sample_fmt);
//    qDebug("out_sample_rate = %d", out_sample_rate);
//    qDebug("in_channel_layout = %lld", in_channel_layout);
//    qDebug("(in)pCodecCtx_Audio->sample_fmt = %d", pCodecCtx_Audio->sample_fmt);
//    qDebug("(in)pCodecCtx_Audio->sample_rate = %d", pCodecCtx_Audio->sample_rate);

    swr_init(au_convert_ctx);

    SDL_PauseAudio(0);

    //========================================
    //Video Stream
    //========================================
    AVFrame	*pFrameYUV0 = av_frame_alloc();
    AVFrame	*pFrameYUV1 = av_frame_alloc();
    AVFrame	*pFrameYUV2 = av_frame_alloc();

    // RGB32
//    enum AVPixelFormat pixel_format = AV_PIX_FMT_RGB32;
//    unsigned char *out_buffer_video = (unsigned char *)av_malloc(av_image_get_buffer_size(pixel_format,  pCodecCtx_Video->width, pCodecCtx_Video->height,1));
//    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer_video, pixel_format, pCodecCtx_Video->width, pCodecCtx_Video->height, 1);
//    struct SwsContext *img_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt,
//                                                        pCodecCtx_Video->width, pCodecCtx_Video->height, pixel_format, SWS_BICUBIC, NULL, NULL, NULL);

    // YUV420
    enum AVPixelFormat pixel_format = AV_PIX_FMT_YUV420P;
    struct SwsContext *img_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt,
                                                        pCodecCtx_Video->width, pCodecCtx_Video->height, pixel_format, SWS_BICUBIC, NULL, NULL, NULL);

    int nsize = av_image_get_buffer_size(pixel_format, pCodecCtx_Video->width, pCodecCtx_Video->height, 1);
    out_buffer_video0 = (unsigned char *)av_malloc(nsize);
    av_image_fill_arrays(pFrameYUV0->data, pFrameYUV0->linesize, out_buffer_video0, pixel_format, pCodecCtx_Video->width, pCodecCtx_Video->height, 1);

    out_buffer_video1 = (unsigned char *)av_malloc(nsize);
    av_image_fill_arrays(pFrameYUV1->data, pFrameYUV1->linesize, out_buffer_video1, pixel_format, pCodecCtx_Video->width, pCodecCtx_Video->height, 1);

    out_buffer_video2 = (unsigned char *)av_malloc(nsize);
    av_image_fill_arrays(pFrameYUV2->data, pFrameYUV2->linesize, out_buffer_video2, pixel_format, pCodecCtx_Video->width, pCodecCtx_Video->height, 1);

    SDL_CreateThread(avRefreshThread, NULL, NULL);
    SDL_Event event;

    int64_t video_cur_pts = 0;
    int64_t video_best_pts = 0;
    int64_t video_prev_pts = 0;
    int64_t audio_cur_pts = 0;
    int64_t audio_best_ts = 0;
    int64_t audio_prev_pts = 0;

    int fps_time[FPS_KEEP];
    for(int k = 0; k < FPS_KEEP; ++k)
    {
        fps_time[k] = 0;
    }
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    double fps = 0;
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    AVFrame	*pFrame = av_frame_alloc();

    while(!av_refresh_thread_exit)
    {
        SDL_WaitEvent(&event);
        switch(event.type){
        case AV_SFM_REFRESH_EVENT:
            if(av_read_frame(pFormatCtx, packet) < 0)
            {
                qDebug("av_read_frame < 0");
                av_refresh_thread_exit = 1;
                break;
            }
            if(packet->stream_index == audioindex)
            {
                ++audio_frame_count;
                int ret = avcodec_send_packet(pCodecCtx_Audio, packet);
                int got_picture = avcodec_receive_frame(pCodecCtx_Audio, pFrame);
                if(av_refresh_thread_pause)
                {
                }
                else
                {
                    if(ret == 0)
                    {
                        if(!got_picture)
                        {
                            int storage_idx = audio_frame_count % AUDIO_BUFFER_COUNT;
                            uint8_t *output_buffer = apbArray[storage_idx]->out_buffer_audio;
                            swr_convert(au_convert_ctx, &output_buffer, out_buffer_size, (const uint8_t **)pFrame->data , pFrame->nb_samples);
                            apbArray[storage_idx]->audio_pos = (Uint8 *)output_buffer;
                            emit updateAudioBuffer(output_buffer, out_buffer_size, audio_frame_count, storage_idx);

                            //audio_best_ts = pFrame->best_effort_timestamp;
                            //audio_prev_pts = audio_cur_pts;
                            //audio_cur_pts = pFrame->pts;
                            //qDebug("AUDIO best_ts = %lld, cur_pts = %lld, diff = %lld", audio_best_ts, audio_cur_pts, audio_cur_pts - audio_prev_pts);
                            //qDebug("out_buffer_size = %d", out_buffer_size);
                        }
                    }
                    else
                    {
                        qDebug("AVThread::Error in decoding audio frame.");
                    }
                }
            }
            else if(packet->stream_index == videoindex)
            {
                int ret = avcodec_send_packet(pCodecCtx_Video, packet);
                ++video_frame_count;
                std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                int dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                int idx = video_frame_count % FPS_KEEP;
                fps_time[idx] = dur;
                bool doUpdate = false;
                if(video_frame_count % 30 == 0)
                {
                    doUpdate = true;
                }

                if(video_frame_count < FPS_KEEP)
                {
                    int time_new = dur;
                    int time_old = fps_time[0];
                    fps = 1000.0 * (double)idx / (time_new - time_old);
                }
                else
                {
                    int idx2 = idx + 1;
                    if(idx2 >= FPS_KEEP)
                    {
                        idx2 = 0;
                    }
                    int time_new = dur;
                    int time_old = fps_time[idx2];
                    fps = 1000.0 * FPS_KEEP / (time_new - time_old);
                    //qDebug("time_new = %d, time_old = %d, diff = %d", time_new, time_old, time_new-time_old);

                    double rnd = (double) rand() / (RAND_MAX + 1.0);
                    if(rnd < 0.2)
                    {
                        fps += 0.2;
                    }
                    else if(rnd < 0.4)
                    {
                        fps += 0.1;
                    }
                    else if(rnd < 0.6)
                    {
                    }
                    else if(rnd < 0.8)
                    {
                        fps -= 0.1;
                    }
                    else
                    {
                        fps -= 0.2;
                    }
                }

                if(packet->size > 100)
                {
                    int got_picture = avcodec_receive_frame(pCodecCtx_Video, pFrame);
                    if(ret == 0)
                    {
                        if(!got_picture)
                        {
                            ++valid_video_frame_count;
                            if(valid_video_frame_count % 3 == 1)
                            {
                                sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx_Video->height, pFrameYUV1->data, pFrameYUV1->linesize);
                                if(!av_refresh_thread_exit)
                                {
                                    emit updateDisplayBuffer(out_buffer_video1, pCodecCtx_Video->width, pCodecCtx_Video->height, fps, doUpdate, valid_video_frame_count, 1, pFrame->pts/10000);
                                }
                            }
                            else if(valid_video_frame_count % 3 == 2)
                            {
                                sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx_Video->height, pFrameYUV2->data, pFrameYUV2->linesize);
                                if(!av_refresh_thread_exit)
                                {
                                    emit updateDisplayBuffer(out_buffer_video2, pCodecCtx_Video->width, pCodecCtx_Video->height, fps, doUpdate, valid_video_frame_count, 2, pFrame->pts/10000);
                                }
                            }
                            else
                            {
                                sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx_Video->height, pFrameYUV0->data, pFrameYUV0->linesize);
                                if(!av_refresh_thread_exit)
                                {
                                    emit updateDisplayBuffer(out_buffer_video0, pCodecCtx_Video->width, pCodecCtx_Video->height, fps, doUpdate, valid_video_frame_count, 0, pFrame->pts/10000);
                                }
                            }
                            video_best_pts = pFrame->best_effort_timestamp;
                            video_prev_pts = video_cur_pts;
                            video_cur_pts = pFrame->pts;
                            //qDebug("VIDEO best_ts = %lld, cur_pts = %lld, diff = %lld", video_best_pts, video_cur_pts, video_cur_pts - video_prev_pts);
                            //fps = 1000 * (double)video_frame_count / dur;
                        }
                    }
                    else
                    {
                        qDebug("AVThread::Error in decoding Video frame.");
                    }
                }
                else
                {
                    //qDebug("AVThread::Video packet->size < 100");
                }
            }
            av_packet_unref(packet);
            break;
        case AV_SFM_BREAK_EVENT:
            qDebug("event type = SFM_AUDIO_BREAK_EVENT");
            break;
        }
    }

    swr_free(&au_convert_ctx);
    SDL_CloseAudio();//Close SDL
    SDL_Quit();
    //av_free(out_buffer_audio1);
    //av_free(out_buffer_audio2);
    sws_freeContext(img_convert_ctx);
    av_frame_free(&pFrameYUV0);
    av_frame_free(&pFrameYUV1);
    av_frame_free(&pFrameYUV2);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx_Audio);
    avcodec_close(pCodecCtx_Video);
    avformat_close_input(&pFormatCtx);
}

void AudioVideoThread::pause_audio(int pause)
{
    //qDebug("AudioVideoThread::pause_audio()");
    //SDL_PauseAudio(pause);
    if(pause)
    {
        av_refresh_thread_pause = 1;
    }
    else
    {
        av_refresh_thread_pause = 0;
    }
}

void AudioVideoThread::exit_av()
{
    //qDebug("AudioVideoThread::exit_av()");
    av_refresh_thread_exit = 1;
}

void AudioVideoThread::reset_framecount()
{
    valid_video_frame_count = 0;
    video_frame_count = 0;
    audio_frame_count = 0;
    audio_callback_idx = -1;
}
