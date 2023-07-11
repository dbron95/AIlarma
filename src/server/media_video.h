#ifndef AAVT_MEDIA_VIDEO_H
#define AAVT_MEDIA_VIDEO_H

#ifndef AAVT_MEDIA_NO_VIDEO

#include <string>
#include "BaseBuffer.h"
#if AAVT_QT_SUPPORT
#include <QtCore>
#include <QString>
#endif


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


typedef enum VideoError
{
    VE_Ok  = 0,
    VE_FileOpen,
    VE_NoOpened,
    VE_NoStreams,
    VE_NoVideoStream,
    VE_NoAudioStream,
    VE_Codec,
    VE_UnsupportedFrameSize,
    VE_Memory,
    VE_VideoEnd,
    VE_NoFrame,
    VE_BadParams,

    VE_FormatUnsupported,
    VE_VideoStreamMissing,
    VE_AudioStreamMissing,
    VE_CodecUnsupported,
    VE_FrameAction,
    VE_Decoder,
    VE_Encoder,
    VE_VideoPacketBufferEmpty,
    VE_NotInitialized,
    VE_AlreadyInitialized,
    VE_FileWrite,
    VE_NotSupported,
    VE_FailtoWriteAudioFrame,
    VE_Generic
} VideoError;

inline bool VE_IsOk ( VideoError error ) { return error == VE_Ok; }

typedef enum VideoCodec
{
    VT_NONE = -1,
    VC_UNCOMPRESSED = 0,
    VC_MP4,
    VC_X264,
    VC_LAST   //!< Ultimo valor
} VideoCodec;

typedef enum VideoCodecSpeed
{
    VCS_VERYSLOW = 0,
    VCS_SLOWER,
    VCS_SLOW,
    VCS_MEDIUM,  // Default
    VCS_FAST,
    VCS_FASTER,
    VCS_VERYFAST,
    VCS_SUPERFAST,
    VCS_ULTRAFAST,
    VCS_LAST   //!< Ultimo valor
} VideoCodecSpeed;


typedef enum VideoParams
{
    VP_NONE = 0,
    VP_SIZE,      // Set width and size
    VP_WIDTH,     // Set width
    VP_HEIGHT,    // Set height
    VP_COLOR,     // Video in color
    VP_MONO,      // Video in mono
    VP_BITRATE,   // Bitrate in thousands of kbits
    VP_FRAMERATE, // In frames per second
    VP_THREADED,  //
    VP_SEEK,      //
    VP_QUALITY,   // QUALITY INDEX (0-64)
    // On X264, 0 means "lossless"
    VP_LAST
} VideoParams;

typedef enum VideoInfo
{
    VI_NONE = 0,
    VI_SIZE,      // Get width and size
    VI_WIDTH,     // Get width
    VI_HEIGHT,    // Get height
    VI_BITRATE,   // Bitrate in thousands of kbits
    VI_FRAMERATE, // In frames per second
    VI_LENGTH,    // Length in frames
    VI_TIMELENGTH, // Length in seconds
    VI_LAST
} VideoInfo;

class CAMUTILS_EXPORT Media_Video {
public:
    Media_Video();    //!< Default constructor
    ~Media_Video();   //!< Default destructor

    // Opens an existing video file, or creates a new video file.
    //   When creating file, output format (.avi,.mp4, ...) is extracted from name
    //   filename is the file name
    //   create must be set to true in case of creating a file
    // Return true is everything was OK
    VideoError Open ( const std::string & filename, bool create = false );
    // Closes a file
    // Return true is everything was OK
    bool Close ( void );

    // Tell is the object has a file (reading or writting)
    bool IsOpened ( void );

    bool IsReading ( void );    // Video is in read mode
    bool IsWritting ( void );   // Video is in write mode

    // Interface for writting videos
    bool SetParams ( int width,     //!< Width of the image
                     int height,    //!< heioght of the image
                     bool colormode, //!< Color fo the video: false is mono, true is in color
                     double bitrate, //!< Bitrate in thousands of bits
                     double framerate, //<! Frame rate in frames/second
                     VideoCodec codec );

    // operator for set video parameters
    Media_Video & operator () ( const VideoCodec codec );
    Media_Video & operator () ( const VideoCodecSpeed codecspeed );
    Media_Video & operator () ( const VideoParams p );
    Media_Video & operator () ( const VideoParams p, int v );
    Media_Video & operator () ( const VideoParams p, int v1, int v2 );
    Media_Video & operator () ( const VideoParams p, double v );

    // Interface for get video information
    Media_Video & operator () ( const VideoInfo p, std::string & v );
    Media_Video & operator () ( const VideoInfo p, int & v );
    Media_Video & operator () ( const VideoInfo p, int & v1, int & v2 );
    Media_Video & operator () ( const VideoInfo p, double & v );

    // Write
    VideoError Add (Buffer8u & buffer );
    VideoError Add (BufferRGBA8 &buffer );
    VideoError Add (BufferRGB8 &buffer );

    Media_Video & operator << (Buffer8u & buffer );
    Media_Video & operator << (BufferRGBA8 &buffer );
    Media_Video & operator << (BufferRGB8 &buffer );
    // Read
    VideoError Get (Buffer8u & buffer, int index = -1 );
    VideoError Get (BufferRGBA8 & buffer, int index = -1 );
    VideoError Get (BufferRGB8 & buffer, int index = -1 );

    Media_Video & operator >> ( Buffer8u & buffer );
    Media_Video & operator >> ( BufferRGBA8 & buffer );
    Media_Video & operator >> ( BufferRGB8 & buffer );

    int Width();
    int Height();
    int FrameRate();
    double Duration();
    void readAgain();
private:
    bool InternalInit ( void );
    bool InternalDestroy ( void );
    VideoError InternalVCodecOpen ( void );
    AVCodec *  InternalVCodecSetupMP4 (AVDictionary * & dict );
    AVCodec *  InternalVCodecSetupX264 (AVDictionary * & dict );
    int        InternalEncodeVideoFrame ( AVFrame * frame,
                                          bool key_frame,
                                          double timestamp );
    VideoError InternalDecodeVideoFrame ( int frameindex );
    VideoError InternalSeekVideoFrame ( int frameindex );
    bool       InternalSetSWSBuffer8u ( const Buffer8u & buffer );
    bool       InternalSetSWSBufferRGBA (const BufferRGBA8 &buffer );
    bool       InternalSetSWSBufferRGB (const BufferRGB8 &buffer );
private:

    bool   mIsReading;
    bool   mIsWritting;
    int    mWidth;
    int    mHeight;
    double mBitrate;
    double mFramerate;
    int    mQuality;
    double mDurationSeconds;
    VideoCodec mVideoOutputCodec;
    bool   mIsColor;
    bool   mIsMono;
    VideoCodecSpeed mCodecSpeed;

    AVFormatContext * mFormatCtx;
    AVOutputFormat *  mOutputFmt;
    AVStream *        mVideoStream;
    AVCodec *         mVideoCodec;

    AVFrame *         mStreamFrame;
    u8 *        mStreamFrameBuffer;
    u8 *        mCodecBuffer;
    int               mCodecBufferSize;
    int               mFrameCount;
    int               mInputVideoStreamIdx;

    // SwScale contexts for video transformation
    struct SwsContext * mSwsFrame;
    struct SwsContext * mSwsBuffer8u;
    struct SwsContext * mSwsBufferRGBA;
    struct SwsContext * mSwsBufferRGB;

};

}//end namespace media
}//end namespace arq

#endif // AAVT_MEDIA_NO_VIDEO

#endif // AAVT_MEDIA_VIDEO_H

