#ifndef BASEBUFFER_H
#define BASEBUFFER_H

#include <typeinfo>
#include <typeindex>
#include <vector>
#include <QImage>
#ifndef Q_MOC_RUN
#include "basictypes.h"
#include <boost/smart_ptr.hpp>
#endif
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include "camutils_export.h"

#include <boost/config/compiler/gcc.hpp>


namespace arq{
namespace media{

//
// Virtual class to be able to do upcasting and downcasting
//
class BaseBuffer
{
public:
    virtual ~BaseBuffer() {}
    inline size_t BufferCode () { return typeid(*this).hash_code(); }
};


//
// Buffer interface
// We are going to merge LTI and OpenCV
//
template < class BASE_TYPE, int MAT_TYPE >
class CAMUTILS_EXPORT Buffer : public BaseBuffer
{
public:
    typedef BASE_TYPE   _base;
    typedef BASE_TYPE * _pbase;

public:  // Miembros
    // Constructor y destructor por defecto
    Buffer ( void );
    Buffer ( int x, int y, BASE_TYPE * data = NULL );
    Buffer ( const Buffer<BASE_TYPE,MAT_TYPE> & buffer );
    ~Buffer ( );
    Buffer<BASE_TYPE,MAT_TYPE>& operator=(const Buffer<BASE_TYPE,MAT_TYPE> other);
    // Crear y destruir im�genes
    bool Create ( int x, int y, BASE_TYPE * data = NULL );
    bool Destroy ( void );

    // Clonar im�genes
    bool Clone ( const Buffer<BASE_TYPE,MAT_TYPE> & buffer );

    // Copy data from different sources
    bool CopyFrom ( BASE_TYPE * buffer, int width, int height, int widthsize );
    bool CopyFrom ( cv::Mat & mat );
    bool Crop ( Buffer<BASE_TYPE,MAT_TYPE> & src, int origin_x, int origin_y );

    //! Check the consistency of the buffer
    bool IsValid ( void ) const;

    // Access to image data

    cv::Mat &   Mat ( void ) const;

    QImage & QtImg( void );

    BASE_TYPE * Data ( void ) const;
    BASE_TYPE * DataRow ( int row ) const;
    int         DataSize ( void ) const;
    BASE_TYPE & Pixel ( int x, int y );
    BASE_TYPE   ReadPixel ( int x, int y ) const;
    int         Width ( void ) const;
    int         Height ( void ) const;

private:
    bool CreateInternalData ( int width, int height, BASE_TYPE * external_data = NULL );
    void DestroyInternalData ( void );

    // Internal data
    bool mExternalData;       // Indicates if data has been internally allocated or externally
    int          mWidth;
    int          mHeight;
    BASE_TYPE *  mData;       // Full image data
    BASE_TYPE ** mDataInRows; // Fast row access
    int          mDataSize;   // Data size

    // Merged types
    //boost::scoped_ptr<cv::Mat>  mMat;
    cv::Mat  * mMat=NULL;
    boost::scoped_ptr<QImage>   mQImage;
};

template<class BASE_TYPE, int MAT_TYPE>
Buffer<BASE_TYPE, MAT_TYPE>::Buffer()
    :mExternalData(false),mWidth(0),mHeight(0),mData(NULL),mDataInRows(NULL),mDataSize(0),mMat(NULL)
{

}

template<class BASE_TYPE, int MAT_TYPE>
Buffer<BASE_TYPE, MAT_TYPE>::Buffer(int x, int y, BASE_TYPE *data)
    :mExternalData(false),mWidth(0),mHeight(0),mData(NULL),mDataInRows(NULL),mDataSize(0),mMat(NULL)
{
    Create ( x, y, data);
}

template<class BASE_TYPE, int MAT_TYPE>
Buffer<BASE_TYPE, MAT_TYPE>::~Buffer()
{
    Destroy();
}

template<class BASE_TYPE, int MAT_TYPE>
Buffer<BASE_TYPE, MAT_TYPE> &Buffer<BASE_TYPE, MAT_TYPE>::operator=(const Buffer<BASE_TYPE, MAT_TYPE> other)
{
    if (this == &other)
        return *this;
    this->Clone(other);
    return *this;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::Create(int x, int y, BASE_TYPE *data)
{
    // First, we must destroy any previous image
    Destroy();

    // We must create the InternalData
    if ( CreateInternalData(x,y,data) )
    {
        // OpenCV Mat structure
        mMat  = new cv::Mat ( mHeight, mWidth, MAT_TYPE, static_cast<unsigned char*>((void*)mData) , cv::Mat::AUTO_STEP);
    }
    if ( mMat)
        return true;

    Destroy();
    return false;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::Destroy()
{
    mQImage.reset();

    if ( mMat )
    {
        delete mMat;
        mMat = NULL;
    }

    DestroyInternalData();

    return true;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::CopyFrom(BASE_TYPE *buffer, int width, int height, int widthsize)
{
    int wsize_in  = std::min(int(width*sizeof(BASE_TYPE)),widthsize);
    int wsize_out = sizeof(BASE_TYPE)*mWidth;
    int wsize     = std::min(wsize_in,wsize_out);

    height = std::min(height,mHeight);

    _pbase src = (_pbase)buffer;
    _pbase dst = mData;

    if ( (width == mWidth) &&
         (height == mHeight) &&
         (wsize_in == wsize_out) )
        memcpy ( dst, src, mDataSize );
    else
    {
        for ( int i = 0 ; i < height ; i++ )
        {
            memcpy ( dst, src, wsize );
            dst += wsize_out;
            src += wsize_in;
        }
    }

    return true;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::CopyFrom(cv::Mat &mat)
{
    if ( !this->IsValid() )
        this->Create(mat.cols,mat.rows);

    if ( mat.size != mMat->size )
        return false;

    if ( mMat->type() == mat.type() )
    {
        mat.copyTo(*mMat);
        return true;
    }

    if ( (mMat->type() == CV_8UC4) &&
         ((mat.type() == CV_8SC3) || (mat.type() == CV_8UC3)) )
    {
        cv::cvtColor ( mat, *mMat, cv::COLOR_RGB2RGBA );
        return true;

    }
    if ( (mat.type() == CV_8UC4) &&
         ((mMat->type() == CV_8SC3) || (mMat->type() == CV_8UC3)) )
    {
        cv::cvtColor ( mat, *mMat, cv::COLOR_RGBA2RGB );
        return true;

    }
    return false;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::IsValid() const
{
    // Primero chequeamos si hay informacion
    if ( !mData || !mMat )
        return false;
    // Y despues chequeamos si la informacion es consistente
    if ((void*)mData != (void*)mMat->data)
        return false;
    return true;
}

template<class BASE_TYPE, int MAT_TYPE>
cv::Mat &Buffer<BASE_TYPE, MAT_TYPE>::Mat() const
{
    return *mMat;
}

template<class BASE_TYPE, int MAT_TYPE>
QImage &Buffer<BASE_TYPE, MAT_TYPE>::QtImg()
{
    // Si el objetvo est� creado
    if ( !mQImage.get() )
    {
        if(MAT_TYPE == CV_8UC1 )
        {
            mQImage.reset(new QImage(static_cast<unsigned char *>((void*)mData),mWidth,mHeight,QImage::Format_Indexed8));
            mQImage->setColorCount(256);
            for ( int i = 0 ; i < 256 ; i++ )
                mQImage->setColor(i,qRgb(i,i,i));
        }
        if ( MAT_TYPE == CV_8UC4 ){
            mQImage.reset(new QImage(static_cast<unsigned char*>((void*)mData),mWidth,mHeight,QImage::Format_RGBA8888));
            *mQImage=mQImage->rgbSwapped();
        }
        if ( MAT_TYPE == CV_8UC3 ){
            mQImage.reset(new QImage(static_cast<unsigned char*>((void*)mData),mWidth,mHeight,QImage::Format_BGR888));
        }
    }
    return *mQImage;
}

template<class BASE_TYPE, int MAT_TYPE>
int Buffer<BASE_TYPE, MAT_TYPE>::DataSize() const
{
    return mDataSize;
}

template<class BASE_TYPE, int MAT_TYPE>
BASE_TYPE *Buffer<BASE_TYPE, MAT_TYPE>::Data() const
{
    return mData;
}

template<class BASE_TYPE, int MAT_TYPE>
BASE_TYPE *Buffer<BASE_TYPE, MAT_TYPE>::DataRow(int row) const
{
    return mDataInRows[row];
}

template<class BASE_TYPE, int MAT_TYPE>
BASE_TYPE &Buffer<BASE_TYPE, MAT_TYPE>::Pixel(int x, int y)
{
    return mDataInRows[y][x];
}

template<class BASE_TYPE, int MAT_TYPE>
BASE_TYPE Buffer<BASE_TYPE, MAT_TYPE>::ReadPixel(int x, int y) const
{
    return mDataInRows[y][x];
}

template<class BASE_TYPE, int MAT_TYPE>
int Buffer<BASE_TYPE, MAT_TYPE>::Width() const
{
    return mWidth;
}

template<class BASE_TYPE, int MAT_TYPE>
int Buffer<BASE_TYPE, MAT_TYPE>::Height() const
{
    return mHeight;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::CreateInternalData(int width, int height, BASE_TYPE *external_data)
{
    if ( external_data )
    {
        mExternalData = true;
        mData = external_data;
    }
    else
    {
        mExternalData = false;
        mData = new _base[width*height];
    }
    //qInfo()<<"sizeof basetype"<<sizeof(BASE_TYPE);
    mDataSize = sizeof(BASE_TYPE)*width*height;

    if ( mData )
    {
        mDataInRows = new _pbase[height];
        for ( int i = 0 ; i < height ; i++ )
            mDataInRows[i] = &mData[width*i];
    }

    if ( mData && mDataInRows )
    {
        mWidth = width;
        mHeight = height;
        return true;
    }

    DestroyInternalData();
    return false;
}

template<class BASE_TYPE, int MAT_TYPE>
void Buffer<BASE_TYPE, MAT_TYPE>::DestroyInternalData()
{
    mWidth  = 0;
    mHeight = 0;
    if ( mDataInRows )
    {
        delete [] mDataInRows;
        mDataInRows = NULL;
    }
    if ( (!mExternalData) && (mData) )
        delete [] mData;
    mData = NULL;
    mExternalData = false;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::Crop(Buffer<BASE_TYPE, MAT_TYPE> &src, int origin_x, int origin_y)
{
    // In the future we need to put size checks
    for ( int y = 0 ; y < mHeight ; y++ )
        for ( int x = 0 ; x < mWidth ; x++ )
            Pixel(x,y) = src.Pixel(x+origin_x,y+origin_y);
    return true;
}

template<class BASE_TYPE, int MAT_TYPE>
bool Buffer<BASE_TYPE, MAT_TYPE>::Clone(const Buffer<BASE_TYPE, MAT_TYPE> &buffer)
{
    if ( ! buffer.IsValid() )
        return false;
    if ( mData == buffer.mData ) // Autoclone no allowed...
        return true;
    Destroy();
    if ( !Create ( buffer.Width(), buffer.Height() ) )
        return false;
    memcpy ( mData, buffer.mData, buffer.mDataSize );
    return true;
}

template<class BASE_TYPE, int MAT_TYPE>
Buffer<BASE_TYPE, MAT_TYPE>::Buffer(const Buffer<BASE_TYPE, MAT_TYPE> &buffer)
    :mExternalData(false),mWidth(0),mHeight(0),mData(NULL),mDataInRows(NULL),mDataSize(0),mMat(NULL)
{
    Clone ( buffer );
}


// Single channel
typedef Buffer<u8,CV_8UC1>       Buffer8u;
typedef Buffer<s8,CV_8SC1>     Buffer8s;
typedef Buffer<u16,CV_16UC1> Buffer16u;
typedef Buffer<s16,CV_16SC1> Buffer16s;
typedef Buffer<s32,CV_32SC1>   Buffer32s;
typedef Buffer<f32,CV_32FC1>     Buffer32f;
// Multichannel
typedef Buffer<rgb8,CV_8UC3> BufferRGB8;
typedef Buffer<rgba8,CV_8UC4> BufferRGBA8;

const size_t Buffer8uCode = typeid(Buffer8u).hash_code();
const size_t Buffer8sCode = typeid(Buffer8s).hash_code();
const size_t Buffer16Code = typeid(Buffer16u).hash_code();
const size_t Buffer16sCode = typeid(Buffer16s).hash_code();
const size_t Buffer32uCode = typeid(Buffer32s).hash_code();
const size_t Buffer32sCode = typeid(Buffer32f).hash_code();
const size_t BufferRGBA8Code = typeid(BufferRGBA8).hash_code();

// Pointer type
typedef boost::shared_ptr<Buffer8u>    Buffer8uPtr;
typedef boost::shared_ptr<Buffer8s>    Buffer8sPtr;
typedef boost::shared_ptr<Buffer16u>   Buffer16uPtr;
typedef boost::shared_ptr<Buffer16s>   Buffer16sPtr;
typedef boost::shared_ptr<Buffer32s>   Buffer32sPtr;
typedef boost::shared_ptr<Buffer32f>   Buffer32fPtr;
typedef boost::shared_ptr<BufferRGBA8> BufferRGBA8Ptr;
typedef boost::shared_ptr<BufferRGB8> BufferRGB8Ptr;

typedef std::vector<Buffer8uPtr>    VecBuffer8u;
typedef std::vector<Buffer8sPtr>    VecBuffer8s;
typedef std::vector<Buffer16uPtr>   VecBuffer16u;
typedef std::vector<Buffer16sPtr>   VecBuffer16s;
typedef std::vector<Buffer32sPtr>   VecBuffer32s;
typedef std::vector<BufferRGBA8Ptr> VecBufferRGBA8;
typedef std::vector<BufferRGB8Ptr>  VecBufferRGB8;




}//end namespace media
}//end namespace arq

#endif // BASEBUFFER_H
