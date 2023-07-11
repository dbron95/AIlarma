#include "include/media_video.h"
#include <QDebug>
//#include <math.h>
#ifndef Q_MOC_RUN
#include <boost/algorithm/string/trim.hpp>
#include <boost/format.hpp>
#endif

#include <math.h>

extern "C" {
    double __exp_finite(double x) { return exp(x); }
    double __exp2_finite(double x) { return exp2(x); }
    double __log_finite(double x) { return log(x); }
    double __log2_finite(double x) { return log2(x); }
    double __log10_finite(double x) { return log10(x); }
    double __pow_finite(double x, double y) { return pow(x, y); }
    double __atan2_finite(double x, double y) { return atan2(x, y);}

    float __expf_finite(float x) { return expf(x); }
    float __exp2f_finite(float x) { return exp2f(x); }
    float __logf_finite(float x) { return logf(x); }
    float __log2f_finite(float x) { return log2f(x); }
    float __powf_finite(float x, float y) { return powf(x, y); }
}


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/dict.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/fifo.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avstring.h>
}

namespace arq{
namespace media{


static void
MediaVideoCallback ( void *, int level, const char * fmt, va_list varg )
{
    char str[256];
    str[0] = 0;
    vsnprintf( str, sizeof(str), fmt, varg );
    std::string stdstr = str;
    if ( level > 32){
        //qDebug()<<"Video"<<QString::fromStdString(boost::trim_right_copy(stdstr));
    }else if ( level > 24 ){
        //qInfo()<<"Video"<<QString::fromStdString(boost::trim_right_copy(stdstr));
    }else if ( level > 16 ){
        //qWarning()<<"Video"<<QString::fromStdString(boost::trim_right_copy(stdstr));
    }else{
        //qWarning()<<"Video"<<QString::fromStdString(boost::trim_right_copy(stdstr));
    }
}

class FFmpegInititalizer
{
public:
    FFmpegInititalizer()
    {
        av_register_all();
        avdevice_register_all();
        avcodec_register_all();
        avformat_network_init();
        av_log_set_callback(MediaVideoCallback);
    }
};

static FFmpegInititalizer _ff_initializer;

template <class T>
inline void
AVFreeAndCleanPtr ( T & t )
{
    if ( t != NULL )
    {
        av_free ( t );
        t = NULL;
    }
}

inline void
av_free_clean ( AVFrame * & frame )
{
    if ( frame != NULL )
        av_frame_free ( &frame );
}

inline void
av_free_clean ( AVCodecContext * & codec )
{
    if ( codec != NULL )
        avcodec_close ( codec );
    codec = NULL;
}

inline void
av_free_clean ( AVPacket & pkt )
{
    av_free_packet ( & pkt );
}

inline void
av_free_clean ( AVPacket * & pkt )
{
    if ( pkt != NULL )
        av_free_packet ( pkt );
}

inline void
av_free_clean ( AVFormatContext * & ctx )
{
    if ( ctx != NULL )
        avformat_free_context ( ctx );
    ctx = NULL;
}

const AVRational *
FixTimeBase ( AVCodec * codec, const AVRational & req )
{
    const AVRational *p= codec->supported_framerates;
    const AVRational *best=NULL;
    AVRational best_error;
    best_error.num=INT_MAX;
    best_error.den=1;
    for(; p->den!=0; p++){
        AVRational error= av_sub_q(req, *p);
        if(error.num <0) error.num *= -1;
        if(av_cmp_q(error, best_error) < 0){
            best_error= error;
            best= p;
        }
    }
    return best;
}

Media_Video::Media_Video()
{
    this->InternalInit();
}

Media_Video::~Media_Video()
{
    this->Close();
    this->InternalDestroy();
}

VideoError Media_Video::Open ( const std::string & filename,
                               bool create )
{
    if ( this->IsOpened() )
        return VE_FileOpen;

    if ( create )
    {
        if ( this->mVideoOutputCodec == VT_NONE )
            return VE_NoVideoStream;

        // Get format from filename
        if ( (this->mOutputFmt = av_guess_format(NULL,filename.c_str(),NULL)) == NULL )
            return VE_FormatUnsupported;

        // Allocate the formar context
        //if ( (this->mFormatCtx = avformat_alloc_context()) == NULL )
        //  return VE_Memory;
        if ( (avformat_alloc_output_context2(&this->mFormatCtx,
                                             this->mOutputFmt,NULL,NULL) < 0) &&
             (this->mFormatCtx == NULL) )
            return VE_Memory;


        this->mFormatCtx->oformat = this->mOutputFmt;
        //this->mFormatCtx->timestamp = 0;
        this->mFormatCtx->flags = AVFMT_FLAG_NONBLOCK;
        strncpy ( this->mFormatCtx->filename, filename.c_str(), sizeof(mFormatCtx->filename) );
        if ( (this->mVideoStream = avformat_new_stream( this->mFormatCtx, 0 )) == NULL )
            return VE_Memory;

        //if ( !(this->mOutputFmt->flags & AVFMT_NOFILE) )
        //  if ( avio_open(&mFormatCtx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0 )
        //    return VE_FileWrite;

        VideoError error;
        if ( (error = InternalVCodecOpen()) != VE_Ok )
            return error;

        if (!this->mVideoStream->codec->codec_tag)
        {
            this->mVideoStream->codec->codec_tag = av_codec_get_tag(this->mOutputFmt->codec_tag,
                                                                    this->mVideoStream->codec->codec_id);
        }

        // Create required temporal AVFrame to codify video frames
        this->mStreamFrame = av_frame_alloc();
        this->mStreamFrame->width = this->mVideoStream->codec->width;
        this->mStreamFrame->height = this->mVideoStream->codec->height;
        this->mStreamFrame->format = this->mVideoStream->codec->pix_fmt;
        this->mStreamFrameBuffer = (u8*) av_malloc(
                    avpicture_get_size( this->mVideoStream->codec->pix_fmt,
                                        this->mVideoStream->codec->width,
                                        this->mVideoStream->codec->height ));
        if ( !this->mStreamFrame || !this->mStreamFrameBuffer )
            return VE_Memory;

        avpicture_fill ( (AVPicture*)this->mStreamFrame,
                         this->mStreamFrameBuffer,
                         this->mVideoStream->codec->pix_fmt,
                         this->mVideoStream->codec->width,
                         this->mVideoStream->codec->height );

        // Allocate a coding buffer
        this->mCodecBufferSize = std::max(256*1024,
                                          8*this->mVideoStream->codec->width*
                                          this->mVideoStream->codec->height+10000);
        this->mCodecBuffer = (u8*)av_malloc(mCodecBufferSize);
        if ( !mCodecBuffer )
            return VE_Memory;

        if ( !(this->mFormatCtx->flags & AVFMT_NOFILE) )
            if ( avio_open(&this->mFormatCtx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0 )
                return VE_FileWrite;

        if ( avformat_write_header ( this->mFormatCtx, NULL ) )
            return VE_FileWrite;

        av_dump_format(this->mFormatCtx,0,filename.c_str(),1);
        this->mIsWritting = true;
        return VE_Ok;
    }
    else
    {
        //        // Open file file
        //        if ( avformat_open_input(&this->mFormatCtx,filename.c_str(),NULL,NULL) != 0 )
        //            return VE_FileOpen;


        AVDictionary* opts = NULL;
        av_dict_set(&opts, "stimeout", "100000", 0);

        // Open video file
        if ( avformat_open_input(&this->mFormatCtx, filename.c_str(), nullptr, &opts) != 0 )
                return VE_FileOpen;
        av_dict_free(&opts);



        // Populate stream information
        if ( avformat_find_stream_info ( mFormatCtx, NULL ) < 0 )
            return VE_NoStreams;

        // Finds the first video codec
        for ( int i = 0 ; i < static_cast<int>(this->mFormatCtx->nb_streams) ; i++ )
        {
            if ( (this->mInputVideoStreamIdx < 0) &&
                 (this->mFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) )
                this->mInputVideoStreamIdx = i;

        }

        if ( this->mInputVideoStreamIdx < 0 )
        {
            this->InternalDestroy();
            this->InternalInit();
            return VE_NoVideoStream;
        }

        mVideoCodec = avcodec_find_decoder ( mFormatCtx->streams[mInputVideoStreamIdx]->codec->codec_id );
        if ( !mVideoCodec ||
             (avcodec_open2 ( mFormatCtx->streams[mInputVideoStreamIdx]->codec, mVideoCodec, NULL ) < 0 ) )
        {
            this->InternalDestroy();
            this->InternalInit();
            return VE_Codec;
        }

        // Don't know is it works now
        //mFormatCtx->streams[mInputVideoStreamIdx]->codec->thread_count = 1

        this->mFramerate = static_cast<double>(this->mFormatCtx->streams[this->mInputVideoStreamIdx]->r_frame_rate.num) /
                static_cast<double>(this->mFormatCtx->streams[this->mInputVideoStreamIdx]->r_frame_rate.den);
        this->mDurationSeconds = static_cast<double>(this->mFormatCtx->duration) / static_cast<double>(AV_TIME_BASE);
        this->mWidth = this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->width;
        this->mHeight = this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->height;
        this->mFrameCount = -1;

        av_dump_format ( mFormatCtx, 0, filename.c_str(), 0 );
        this->mIsReading = true;
        return VE_Ok;
    }


    return VE_Generic;
}


bool Media_Video::Close ( void )
{
    if ( this->mIsWritting )
    {
        while ( InternalEncodeVideoFrame(NULL,false,0) >= 0 );
        av_write_trailer ( mFormatCtx );

    }

    if ( this->mIsReading )
    {
        /*
    AVFreeAndCleanPtr ( mCodecBuffer );
    mCodecBufferSize = 0;
    AVFreeAndCleanPtr ( mStreamFrameBuffer );
    av_free_clean ( mStreamFrame );
    if ( this->mFormatCtx &&
         (this->mInputVideoStreamIdx >= 0) )
    {
      avcodec_close(mFormatCtx->streams[mInputVideoStreamIdx]->codec);
      avformat_close_input ( &mFormatCtx );
    }
    mOutputFmt = NULL;
    mVideoStream = NULL;*/
    }

    this->mIsWritting = false;
    this->mIsReading = false;
    InternalDestroy();
    InternalInit();

    return true;
}


bool Media_Video::IsOpened ( void )
{
    return this->mIsReading || this->mIsWritting;
}



bool Media_Video::IsReading ( void )
{
    return this->mIsReading;
}


bool Media_Video::IsWritting ( void )
{
    return this->mIsWritting;
}


bool Media_Video::SetParams ( int width,
                              int height,
                              bool colormode,
                              double bitrate,
                              double framerate,
                              VideoCodec codec )
{
    if ( this->mIsReading )
        return false;

    this->mWidth = width;
    this->mHeight = height;
    this->mBitrate = bitrate;
    this->mFramerate = framerate;
    this->mVideoOutputCodec = codec;
    if ( colormode )
        this->mIsColor = true;
    else
        this->mIsMono = true;

    return true;
}


Media_Video & Media_Video::operator () ( const VideoCodec codec )
{
    if ( !this->IsOpened() )
        this->mVideoOutputCodec = codec;
    return *this;
}

Media_Video & Media_Video::operator () ( const VideoCodecSpeed codecspeed )
{
    if ( !this->IsOpened() )
        this->mCodecSpeed = codecspeed;
    return *this;
}

Media_Video & Media_Video::operator () ( const VideoParams p )
{
    if ( !this->IsOpened() )
        switch ( p )
        {
        case VP_MONO: this->mIsColor = false; this->mIsMono = true; break;
        case VP_COLOR: this->mIsColor = true; this->mIsMono = false; break;
        default:;
        }
    return *this;
}


Media_Video & Media_Video::operator () ( const VideoParams p, int v )
{
    if ( !this->IsOpened() )
        switch ( p )
        {
        case VP_WIDTH: this->mWidth = v; break;
        case VP_HEIGHT: this->mHeight = v; break;
        case VP_BITRATE: this->mBitrate = static_cast<double>(v); break;
        case VP_FRAMERATE: this->mFramerate = static_cast<double>(v); break;
        case VP_QUALITY: this->mQuality = v; break;
        default:;
        }
    return *this;
}

Media_Video &
Media_Video::operator () ( const VideoParams p, int v1, int v2 )
{
    if ( !this->IsOpened() )
        switch ( p )
        {
        case VP_SIZE: this->mWidth = v1; this->mHeight = v2; break;
        default:;
        }
    return *this;
}

Media_Video &
Media_Video::operator () ( const VideoParams p, double v )
{
    if ( !this->IsOpened() )
        switch ( p )
        {
        case VP_BITRATE: this->mBitrate = v; break;
        case VP_FRAMERATE: this->mFramerate = v; break;
        case VP_QUALITY: this->mQuality = static_cast<int>(v); break;
        default:;
        }
    return *this;
}

Media_Video &
Media_Video::operator () ( const VideoInfo p,
                           std::string & v )
{
    return *this;
}

Media_Video &
Media_Video::operator () ( const VideoInfo p,
                           int & v )
{
    if ( this->mIsReading )
        switch ( p )
        {
        case VI_WIDTH: v = this->mWidth; break;
        case VI_HEIGHT: v = this->mHeight; break;
        case VI_LENGTH: v = static_cast<int>(this->mFramerate*this->mDurationSeconds); break;
        default:;
        }
    else if ( this->mIsWritting )
        switch ( p )
        {
        case VI_WIDTH: v = this->mWidth; break;
        case VI_HEIGHT: v = this->mHeight; break;
        default:;
        }
    else
        v = 0;

    return *this;
}

Media_Video &
Media_Video::operator () ( const VideoInfo p,
                           int & v1, int & v2 )
{
    if ( this->mIsReading )
        switch ( p )
        {
        case VI_SIZE:
            v1 = this->mWidth;
            v2 = this->mHeight;
            break;
        default:;
        }
    else if ( this->mIsWritting )
        switch ( p )
        {
        case VI_SIZE: v1 = this->mWidth; v2 = this->mHeight; break;
        default:;
        }
    else
        v1 = v2 = 0;

    return *this;
}

Media_Video &
Media_Video::operator () ( const VideoInfo p,
                           double & v )
{
    if ( this->mIsReading )
        switch ( p )
        {
        case VI_BITRATE:
            v = this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->bit_rate/1000.0;
            break;
        case VI_FRAMERATE: v = this->mFramerate; break;
        case VI_TIMELENGTH: v = this->mDurationSeconds; break;
        case VI_LENGTH: v = std::floor(this->mDurationSeconds*100.0*this->mFramerate)/100.0; break;
        default:;
        }
    else if ( this->mIsWritting )
        switch ( p )
        {
        case VI_BITRATE: v = this->mBitrate; break;
        case VI_FRAMERATE: v = this->mFramerate; break;
        default:;
        }
    return *this;
}



VideoError
Media_Video::InternalVCodecOpen ( void )
{
    AVCodec * codec = NULL;
    AVDictionary * codec_options = NULL;

    switch ( this->mVideoOutputCodec )
    {
    case VC_UNCOMPRESSED:
        //codec = NULL;
        return VE_NotSupported;
        break;
    case VC_MP4:
        codec = InternalVCodecSetupMP4(codec_options);
        break;
    case VC_X264:
        codec = InternalVCodecSetupX264(codec_options);
        break;
    default:
        return VE_NotSupported;
    }

    if ( (codec == NULL) ||
         (avcodec_open2 ( this->mVideoStream->codec, codec, &codec_options ) < 0) )
        return VE_CodecUnsupported;

    return VE_Ok;
}

AVCodec *
Media_Video::InternalVCodecSetupMP4 ( AVDictionary * & dict )
{
    AVCodecContext * codec_ctx;
    AVCodec *        codec;
    AVRational       inv_time_base = av_d2q(this->mFramerate,INT_MAX);

    codec_ctx = this->mVideoStream->codec;
    codec_ctx->codec_id = AV_CODEC_ID_MPEG4;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->thread_count = 1;

    if ( (codec = avcodec_find_encoder ( codec_ctx->codec_id )) == NULL )
        return NULL;

    this->mVideoStream->codec = avcodec_alloc_context3(codec);
    if ( this->mFormatCtx->oformat->flags  & AVFMT_GLOBALHEADER )
        this->mVideoStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if ( !this->mVideoStream->codec->rc_initial_buffer_occupancy )
        this->mVideoStream->codec->rc_initial_buffer_occupancy = this->mVideoStream->codec->rc_buffer_size *3/4;

    codec_ctx = mVideoStream->codec;
    codec_ctx->bit_rate = this->mBitrate*1024;
    codec_ctx->bit_rate_tolerance = this->mBitrate*1024;
    codec_ctx->time_base.num = inv_time_base.den;
    codec_ctx->time_base.den = inv_time_base.num;
    codec_ctx->framerate.num = inv_time_base.num;
    codec_ctx->framerate.den = inv_time_base.den;
    mVideoStream->time_base.num = inv_time_base.den;
    mVideoStream->time_base.den = inv_time_base.num;
    codec_ctx->width = this->mWidth;
    codec_ctx->height = this->mHeight;
    codec_ctx->sample_aspect_ratio    = av_d2q(0*double(this->mWidth)/double(this->mHeight),255);
    this->mVideoStream->sample_aspect_ratio = av_d2q(0*double(this->mWidth)/double(this->mHeight),255);
    //codec_ctx->me_threshold = 0;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    //codec_ctx->intra_dc_precision= 0;
    //codec_ctx->strict_std_compliance = 0;

    if ( codec->supported_framerates )
    {
        // If codec support a limited set of allowed framerartes, choose the
        // most similar
        const AVRational * best = FixTimeBase ( codec, codec_ctx->time_base );
        if ( best )
        {
            codec_ctx->time_base.num = best->num;
            codec_ctx->time_base.den = best->den;
        }
    }

    if ( this->mQuality >= 0 )
    {
        int q = std::min(this->mQuality,31);
        //q = std::max(q,1);
        codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
        //    codec_ctx->flags |= CODEC_FLAG_QSCALE;
        codec_ctx->global_quality = q*FF_LAMBDA_SCALE;
    }

    return codec;

}

AVCodec *
Media_Video::InternalVCodecSetupX264 (AVDictionary * & dict )
{
    AVCodecContext * codec_ctx;
    AVCodec *        codec;
    AVRational       inv_time_base = av_d2q(this->mFramerate,INT_MAX);
    int              codec_speed = static_cast<int>(this->mCodecSpeed);

    codec_ctx = this->mVideoStream->codec;
    codec_ctx->codec_id = AV_CODEC_ID_H264;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->thread_count = 1;
    //codec_ctx->profile = FF_PROFILE_H264_HIGH;


    if ( (codec = avcodec_find_encoder ( codec_ctx->codec_id )) == NULL )
        return NULL;

    this->mVideoStream->codec = avcodec_alloc_context3(codec);
    if ( this->mFormatCtx->oformat->flags  & AVFMT_GLOBALHEADER )
        this->mVideoStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if ( !this->mVideoStream->codec->rc_initial_buffer_occupancy )
        this->mVideoStream->codec->rc_initial_buffer_occupancy = this->mVideoStream->codec->rc_buffer_size *3/4;

    codec_ctx = mVideoStream->codec;
    codec_ctx->bit_rate = this->mBitrate*1024;
    codec_ctx->bit_rate_tolerance = this->mBitrate*1024;
    codec_ctx->time_base.num = inv_time_base.den;
    codec_ctx->time_base.den = inv_time_base.num;
    codec_ctx->framerate.num = inv_time_base.num;
    codec_ctx->framerate.den = inv_time_base.den;
    mVideoStream->time_base.num = inv_time_base.den;
    mVideoStream->time_base.den = inv_time_base.num;
    mVideoStream->avg_frame_rate.num = inv_time_base.num;
    mVideoStream->avg_frame_rate.den = inv_time_base.den;
    av_stream_set_r_frame_rate(mVideoStream,inv_time_base);
    codec_ctx->width = this->mWidth;
    codec_ctx->height = this->mHeight;
    codec_ctx->sample_aspect_ratio    = av_d2q(0*double(this->mWidth)/double(this->mHeight),255);
    mVideoStream->sample_aspect_ratio = av_d2q(0*double(this->mWidth)/double(this->mHeight),255);
    //codec_ctx->me_threshold = 0;
    //codec_ctx->pix_fmt = PIX_FMT_YUV422P;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    //codec_ctx->intra_dc_precision= 0;
    //codec_ctx->strict_std_compliance = 0;
    codec_ctx->profile = FF_PROFILE_H264_HIGH_444_PREDICTIVE;

    if ( codec->supported_framerates )
    {
        // If codec support a limited set of allowed framerartes, choose the
        // most similar
        const AVRational * best = FixTimeBase ( codec, codec_ctx->time_base );
        if ( best )
        {
            codec_ctx->time_base.num = best->num;
            codec_ctx->time_base.den = best->den;
        }
    }

    if ( (codec_speed >= 0) && (codec_speed <=8) )
    {
        const char * speeds [9] = { "veryslow", "slower", "slow", "medium",
                                    "fast", "faster", "veryfast", "superfast",
                                    "ultrafast" };
        av_dict_set(&dict,"preset",speeds[codec_speed],0);
    }

    if ( this->mQuality >= 0 )
    {
        int q = std::min(this->mQuality,63);
        std::string quality = boost::str(boost::format("%i")%q);
        av_dict_set(&dict,"qp",quality.c_str(),0);
    }

    return codec;
}


int
Media_Video::InternalEncodeVideoFrame ( AVFrame * frame,
                                        bool key_frame,
                                        double timestamp )
{
    int encodec_bytes = 0;
    double timebase = 0.0;

    // Frame could be NULL to flush the codec buffer
    if ( frame != NULL )
    {
        if ( key_frame )
            frame->pict_type = AV_PICTURE_TYPE_I;
        else
            //frame->pict_type = AV_PICTURE_TYPE_P;
            frame->pict_type = AV_PICTURE_TYPE_NONE;
        frame->key_frame = key_frame ? 1 : frame->key_frame;
        frame->quality = this->mVideoStream->codec->global_quality;

        if ( timestamp < 0 )
            timestamp = static_cast<double>(this->mFrameCount) / this->mFramerate;
        timebase = (double(this->mVideoStream->codec->time_base.num)/
                    double(this->mVideoStream->codec->time_base.den));
        frame->pts = static_cast<s64>(timestamp/timebase + 0.5);
        frame->pkt_dts = AV_NOPTS_VALUE;
        frame->pkt_pts = AV_NOPTS_VALUE;
    }

    int got_output = 0;
    AVPacket pkt;
    av_init_packet ( &pkt );
    pkt.stream_index = this->mVideoStream->index;
    pkt.data = NULL;
    pkt.size = 0;
    pkt.pts = AV_NOPTS_VALUE;
    if ( this->mVideoStream->codec->coded_frame &&
         this->mVideoStream->codec->coded_frame->pts != AV_NOPTS_VALUE )
        pkt.pts = av_rescale_q ( this->mVideoStream->codec->coded_frame->pts,
                                 this->mVideoStream->codec->time_base,
                                 this->mVideoStream->time_base );
    if ( this->mVideoStream->codec->coded_frame &&
         this->mVideoStream->codec->coded_frame->key_frame )
        pkt.flags |= AV_PKT_FLAG_KEY;

    encodec_bytes = avcodec_encode_video2 ( this->mVideoStream->codec,
                                            &pkt,
                                            frame,
                                            &got_output );
    if (pkt.pts != AV_NOPTS_VALUE)
        pkt.pts =  av_rescale_q(pkt.pts, mVideoStream->codec->time_base, mVideoStream->time_base);
    if (pkt.dts != AV_NOPTS_VALUE)
        pkt.dts = av_rescale_q(pkt.dts, mVideoStream->codec->time_base, mVideoStream->time_base);
    if ( got_output )
    {
        if ( av_write_frame ( mFormatCtx, &pkt ) != 0 )
            encodec_bytes = -1;
        av_free_packet(&pkt);
    }

    if ( encodec_bytes >= 0 )
        this->mFrameCount++;

    if ( (encodec_bytes == 0) && !got_output )
        encodec_bytes = -1;

    return encodec_bytes;
}

VideoError
Media_Video::Add ( Buffer8u & buffer )
{
    if ( !this->mIsWritting )
        return VE_NoOpened;

    double timestamp = -1.0;
    bool   key_frame = this->mFrameCount == 0 ? true : false;

    this->InternalSetSWSBuffer8u ( buffer );

    // From Buffer8u to final AVFrame
    AVFrame * src_frame = av_frame_alloc();
    if ( src_frame == NULL )
        return VE_Memory;
    avpicture_fill ( (AVPicture*)src_frame,
                     (u8*)buffer.Data(),
                     AV_PIX_FMT_GRAY8,
                     buffer.Width(),
                     buffer.Height() );
    sws_scale ( this->mSwsBuffer8u,
                src_frame->data, src_frame->linesize, 0, buffer.Height(),
                this->mStreamFrame->data, this->mStreamFrame->linesize );
    av_free ( src_frame );

    mStreamFrame->key_frame = 0;
    if ( this->InternalEncodeVideoFrame ( this->mStreamFrame,
                                          key_frame, timestamp ) < 0 )
    {
        return VE_Encoder;
    }

    return VE_Ok;
}

VideoError
Media_Video::Add( BufferRGBA8 & buffer )
{
    if ( !this->mIsWritting )
        return VE_NoOpened;

    double timestamp = -1.0;
    bool   key_frame = this->mFrameCount == 0 ? true : false;

    this->InternalSetSWSBufferRGBA ( buffer );

    // From Buffer8u to final AVFrame
    AVFrame * src_frame = av_frame_alloc();
    if ( src_frame == NULL )
        return VE_Memory;
    avpicture_fill ( (AVPicture*)src_frame,
                     buffer.Mat().data,
                     AV_PIX_FMT_BGRA,
                     buffer.Mat().cols,
                     buffer.Mat().rows );

    sws_scale ( this->mSwsBufferRGBA,
                src_frame->data, src_frame->linesize, 0, buffer.Height(),
                this->mStreamFrame->data, this->mStreamFrame->linesize );
    av_free ( src_frame );

    mStreamFrame->key_frame = 0;
    if ( this->InternalEncodeVideoFrame ( this->mStreamFrame,
                                          key_frame, timestamp ) < 0 )
    {
        return VE_Encoder;
    }

    return VE_Ok;
}

VideoError Media_Video::Add(BufferRGB8 &buffer)
{
    if ( !this->mIsWritting )
        return VE_NoOpened;

    double timestamp = -1.0;
    bool   key_frame = this->mFrameCount == 0 ? true : false;

    this->InternalSetSWSBufferRGB ( buffer );

    // From BufferRGB to final AVFrame
    AVFrame * src_frame = av_frame_alloc();
    if ( src_frame == NULL )
        return VE_Memory;
    avpicture_fill ( (AVPicture*)src_frame,
                     buffer.Mat().data,
                     AV_PIX_FMT_BGR24,
                     buffer.Mat().cols,
                     buffer.Mat().rows );

    sws_scale ( this->mSwsBufferRGB,
                src_frame->data, src_frame->linesize, 0, buffer.Height(),
                this->mStreamFrame->data, this->mStreamFrame->linesize );
    av_free ( src_frame );

    mStreamFrame->key_frame = 0;
    if ( this->InternalEncodeVideoFrame ( this->mStreamFrame,
                                          key_frame, timestamp ) < 0 )
    {
        return VE_Encoder;
    }

    return VE_Ok;
}

Media_Video &
Media_Video::operator << ( Buffer8u & buffer )
{
    if ( this->Add(buffer) != VE_Ok )
        buffer.Destroy();
    return *this;
}

Media_Video &
Media_Video::operator << (BufferRGBA8 &buffer )
{
    if ( this->Add(buffer) != VE_Ok )
        buffer.Destroy();
    return *this;
}

Media_Video &
Media_Video::operator << (BufferRGB8 &buffer )
{
    if ( this->Add(buffer) != VE_Ok )
        buffer.Destroy();
    return *this;
}


VideoError
Media_Video::InternalDecodeVideoFrame( int frameindex )
{
    VideoError error = VE_Ok;
    int        frame_rdy = 0;

    frameindex = frameindex;
    if ( this->mStreamFrame)
        av_free_clean(this->mStreamFrame);
    this->mStreamFrame = av_frame_alloc();
    while ( !frame_rdy && VE_IsOk(error) )
    {
        AVPacket pkt;
        if ( av_read_frame ( this->mFormatCtx, &pkt ) < 0 )
        {
            error = VE_VideoEnd;
            av_free_clean(this->mStreamFrame);
            this->mFrameCount = -1;
        }
        else if ( pkt.stream_index != this->mInputVideoStreamIdx )
        {
            av_free_clean(pkt);
            frame_rdy = 0;
        }
        else
        {
            this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->reordered_opaque = pkt.pts;
            if ( avcodec_decode_video2 ( this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec,
                                         this->mStreamFrame,
                                         &frame_rdy,
                                         &pkt ) < 0 )
            {
                av_free_clean(this->mStreamFrame);
                error = VE_NoFrame;
            }
        }
        av_free_clean(pkt);
    }
    if ( frame_rdy )
        this->mFrameCount++;

    return error;
}


VideoError Media_Video::InternalSeekVideoFrame ( int frameindex )
{

    return VE_Generic;
}

VideoError Media_Video::Get ( Buffer8u & buffer,int index )

{
    VideoError error = VE_Ok;

    if ( !this->mIsReading )
        return VE_NoOpened;

    if ( !buffer.IsValid() &&
         !buffer.Create(this->mWidth,this->mHeight) )
    {
        qWarning()<<"Video Unable to create frame in Video::Get";
        return VE_Memory;
    }

    if ( !this->InternalSetSWSBuffer8u(buffer) )
        return VE_Memory;

    if ( (error = InternalDecodeVideoFrame(index)) != VE_Ok )
        return error;

    AVFrame * grayframe = av_frame_alloc();
    int size = avpicture_get_size ( AV_PIX_FMT_GRAY8,buffer.Width(),buffer.Height() );


    if ( !grayframe || (avpicture_fill((AVPicture*)grayframe,buffer.Data(),AV_PIX_FMT_GRAY8,buffer.Width(),buffer.Height() ) < 0) || ( sws_scale(mSwsBuffer8u,mStreamFrame->data, mStreamFrame->linesize,0,this->mHeight,grayframe->data,grayframe->linesize ) < 0 ) )
        error = VE_Generic;

    av_free_clean(grayframe);
    return error;
}


VideoError Media_Video::Get ( BufferRGBA8 & buffer,int index )
{
    VideoError error = VE_Ok;

    if ( !this->mIsReading )
        return VE_NoOpened;

    if ( !buffer.IsValid() &&
         !buffer.Create(this->mWidth,this->mHeight) )
    {
        qWarning()<<"Video Unable to create frame in Video::Get";
        return VE_Memory;
    }

    if ( !this->InternalSetSWSBufferRGBA(buffer) )
        return VE_Memory;

    if ( (error = InternalDecodeVideoFrame(index)) != VE_Ok )
        return error;

    AVFrame * grayframe = av_frame_alloc();
    int size = avpicture_get_size ( AV_PIX_FMT_BGRA,buffer.Width(),buffer.Height() );


    if ( !grayframe || (avpicture_fill((AVPicture*)grayframe,buffer.Mat().data,AV_PIX_FMT_BGRA,buffer.Width(),buffer.Height() ) < 0) || ( sws_scale(mSwsBufferRGBA,mStreamFrame->data, mStreamFrame->linesize,0,this->mHeight,grayframe->data,grayframe->linesize ) < 0 ) )
        error = VE_Generic;

    av_free_clean(grayframe);
    return error;
}

VideoError Media_Video::Get ( BufferRGB8 & buffer,int index )
{
    VideoError error = VE_Ok;

    if ( !this->mIsReading )
        return VE_NoOpened;

    if ( !buffer.IsValid() &&
         !buffer.Create(this->mWidth,this->mHeight) )
    {
        qWarning()<<"Video Unable to create frame in Video::Get";
        return VE_Memory;
    }

    if ( !this->InternalSetSWSBufferRGB(buffer) )
        return VE_Memory;

    if ( (error = InternalDecodeVideoFrame(index)) != VE_Ok )
        return error;

    AVFrame * grayframe = av_frame_alloc();
    int size = avpicture_get_size ( AV_PIX_FMT_BGR24,buffer.Width(),buffer.Height() );


    if ( !grayframe || (avpicture_fill((AVPicture*)grayframe,buffer.Mat().data,AV_PIX_FMT_BGR24,buffer.Width(),buffer.Height() ) < 0) || ( sws_scale(mSwsBufferRGB,mStreamFrame->data, mStreamFrame->linesize,0,this->mHeight,grayframe->data,grayframe->linesize ) < 0 ) )
        error = VE_Generic;

    av_free_clean(grayframe);
    return error;
}


Media_Video & Media_Video::operator >> ( Buffer8u & buffer )
{
    if ( this->Get(buffer) != VE_Ok )
        buffer.Destroy();
    return *this;
}

Media_Video & Media_Video::operator >> ( BufferRGBA8 & buffer )
{
    if ( this->Get(buffer) != VE_Ok )
        buffer.Destroy();
    return *this;
}

Media_Video & Media_Video::operator >> ( BufferRGB8 & buffer )
{
    if ( this->Get(buffer) != VE_Ok )
        buffer.Destroy();
    return *this;
}

int Media_Video::Width()
{
    return mWidth;
}

int Media_Video::Height()
{
    return mHeight;
}

int Media_Video::FrameRate()
{
    return mFramerate;
}

double Media_Video::Duration()
{
    return mDurationSeconds;
}

void Media_Video::readAgain()
{

}

bool Media_Video::InternalSetSWSBuffer8u ( const Buffer8u & buffer )
{
    // First thing is to convert the buffer into a normalized frame
    struct SwsContext * swsctx = NULL;

    if ( this->mIsWritting )
        swsctx = sws_getCachedContext ( this->mSwsBuffer8u,buffer.Width(), buffer.Height(), AV_PIX_FMT_GRAY8,this->mWidth, this->mHeight, this->mVideoStream->codec->pix_fmt,SWS_BICUBIC, NULL, NULL, NULL );



    if ( this->mIsReading )
        swsctx = sws_getCachedContext ( this->mSwsBuffer8u,
                                        this->mWidth, this->mHeight,
                                        this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->pix_fmt,
                buffer.Width(), buffer.Height(), AV_PIX_FMT_GRAY8,
                SWS_BICUBIC, NULL, NULL, NULL );

    if ( swsctx != this->mSwsBuffer8u )
    {
        if ( this->mSwsBuffer8u )
            sws_freeContext ( this->mSwsBuffer8u );
        this->mSwsBuffer8u = swsctx;
    }

    if ( !swsctx )
    {
        qWarning()<<"Video Unable to create SWS 8u";
    }

    return this->mSwsBuffer8u != NULL;
}

bool Media_Video::InternalSetSWSBufferRGBA(const BufferRGBA8 &buffer)
{
    // First thing is to convert the buffer into a normalized frame
    struct SwsContext * swsctx = NULL;

    if ( this->mIsWritting )
        swsctx = sws_getCachedContext ( this->mSwsBufferRGBA,buffer.Width(), buffer.Height(), AV_PIX_FMT_BGRA,this->mWidth, this->mHeight, this->mVideoStream->codec->pix_fmt,SWS_BICUBIC, NULL, NULL, NULL );



    if ( this->mIsReading )
        swsctx = sws_getCachedContext ( this->mSwsBufferRGBA,
                                        this->mWidth, this->mHeight,
                                        this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->pix_fmt,
                buffer.Width(), buffer.Height(),AV_PIX_FMT_BGRA,
                SWS_BICUBIC, NULL, NULL, NULL );

    if ( swsctx != this->mSwsBufferRGBA )
    {
        if ( this->mSwsBufferRGBA )
            sws_freeContext ( this->mSwsBufferRGBA );
        this->mSwsBufferRGBA = swsctx;
    }

    if ( !swsctx )
    {
        qWarning()<<"Video Unable to create SWS RGBA";
    }

    return this->mSwsBufferRGBA != NULL;
}

bool Media_Video::InternalSetSWSBufferRGB(const BufferRGB8 &buffer)
{
    // First thing is to convert the buffer into a normalized frame
    struct SwsContext * swsctx = NULL;

    if ( this->mIsWritting )
        swsctx = sws_getCachedContext ( this->mSwsBufferRGB,buffer.Width(), buffer.Height(), AV_PIX_FMT_BGR24,this->mWidth, this->mHeight, this->mVideoStream->codec->pix_fmt,SWS_BICUBIC, NULL, NULL, NULL );



    if ( this->mIsReading )
        swsctx = sws_getCachedContext ( this->mSwsBufferRGB,
                                        this->mWidth, this->mHeight,
                                        this->mFormatCtx->streams[this->mInputVideoStreamIdx]->codec->pix_fmt,
                buffer.Width(), buffer.Height(),AV_PIX_FMT_BGR24,
                SWS_BICUBIC, NULL, NULL, NULL );

    if ( swsctx != this->mSwsBufferRGB )
    {
        if ( this->mSwsBufferRGB )
            sws_freeContext ( this->mSwsBufferRGB );
        this->mSwsBufferRGB = swsctx;
    }

    if ( !swsctx )
    {
        qWarning()<<"Video Unable to create SWS RGBA";
    }

    return this->mSwsBufferRGB != NULL;
}

bool
Media_Video::InternalInit ( void )
{
    this->mIsReading = false;
    this->mIsWritting = false;
    this->mWidth = -1;
    this->mHeight = -1;
    this->mBitrate = 0.0;
    this->mFramerate = 0.0;
    this->mQuality = -1;
    this->mDurationSeconds = 0.0;
    this->mVideoOutputCodec = VT_NONE;
    this->mCodecSpeed = VCS_MEDIUM;
    this->mIsColor = false;
    this->mIsMono = false;
    this->mFormatCtx = NULL;
    this->mOutputFmt = NULL;
    this->mVideoStream = NULL;
    this->mVideoCodec = NULL;
    this->mStreamFrame = NULL;
    this->mStreamFrameBuffer = NULL;
    this->mCodecBuffer = 0;
    this->mCodecBufferSize = 0;
    this->mFrameCount = 0;
    this->mSwsFrame = NULL;
    this->mSwsBuffer8u = NULL;
    this->mSwsBufferRGBA = NULL;
    this->mSwsBufferRGB = NULL;
    this->mInputVideoStreamIdx = -1;

    return true;
}

bool
Media_Video::InternalDestroy ( void )
{
    AVFreeAndCleanPtr ( mCodecBuffer );
    mCodecBufferSize = 0;
    AVFreeAndCleanPtr ( mStreamFrameBuffer );
    av_free_clean ( mStreamFrame );
    if ( this->mVideoStream &&
         this->mVideoStream->codec )
        avcodec_close ( mVideoStream->codec );
    if ( this->mFormatCtx &&
         (this->mInputVideoStreamIdx >= 0) )
    {
        avcodec_close(mFormatCtx->streams[mInputVideoStreamIdx]->codec);
        avformat_close_input ( &mFormatCtx );
    }
    if ( mFormatCtx )
    {
        for ( unsigned int i = 0 ; i < mFormatCtx->nb_streams ; i++ )
        {
            //av_free_clean ( mFormatCtx->streams[i]->codec );
            //AVFreeAndCleanPtr ( mFormatCtx->streams[i] );
        }
        if ( !(mFormatCtx->oformat->flags & AVFMT_NOFILE) && mFormatCtx->pb )
            avio_closep(&mFormatCtx->pb);
        av_free_clean ( mFormatCtx );
    }
    mOutputFmt = NULL;
    mVideoStream = NULL;
    if ( this->mSwsFrame )
        sws_freeContext ( this->mSwsFrame );
    this->mSwsFrame = NULL;
    if ( this->mSwsBuffer8u )
        sws_freeContext ( this->mSwsBuffer8u );
    this->mSwsBuffer8u = NULL;
    if ( this->mSwsBufferRGBA )
        sws_freeContext ( this->mSwsBufferRGBA );
    this->mSwsBufferRGBA = NULL;
    if ( this->mSwsBufferRGB )
        sws_freeContext ( this->mSwsBufferRGB );
    this->mSwsBufferRGB = NULL;
    return true;
}

}//end namespace media
}//end namespace arq
