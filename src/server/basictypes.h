#ifndef BASICTYPES_H
#define BASICTYPES_H

#include "camutils_export.h"
#ifndef Q_MOC_RUN
#include <boost/cstdint.hpp>
#endif
/**
 * Exact size integers
 */

namespace arq{
namespace media{


typedef boost::int8_t   s8;   //!< Exact size integer 8 bits
typedef boost::int16_t  s16;  //!< Exact size integer 16 bits
typedef boost::int32_t  s32;  //!< Exact size integer 32 bits
typedef boost::int64_t  s64;  //!< Exact size integer 64 bits
typedef boost::uint8_t  u8;   //!< Exact size unsigned integer 8 bits
typedef boost::uint16_t u16;  //!< Exact size unsigned integer 16 bits
typedef boost::uint32_t u32;  //!< Exact size unsigned integer 32 bits
typedef boost::uint64_t u64;  //!< Exact size unsigned integer 64 bits

/**
 * Fast ingegers. Size could be bigger than expected.
 */
typedef boost::int_fast8_t    s8f;   //!< Fast integer 8 bits
typedef boost::int_fast16_t   s16f;  //!< Fast integer 16 bits
typedef boost::int_fast32_t   s32f;  //!< Fast integer 32 bits
typedef boost::int_fast64_t   s64f;  //!< Fast integer 64 bits
typedef boost::uint_fast8_t   u8f;   //!< Fast integer 8 bits
typedef boost::uint_fast16_t  u16f;  //!< Fast integer 16 bits
typedef boost::uint_fast32_t  u32f;  //!< Fast integer 32 bits
typedef boost::uint_fast64_t  u64f;  //!< Fast integer 64 bits

/**
  * Float types
  */
typedef float  f32;
typedef double f64;
typedef long double f128;


/**
  * Complex strcutures.
  */
typedef union CAMUTILS_EXPORT rgba8
{
  u32 value;
  struct rgba
  {
    u8 blue;
    u8 green;
    u8 red;
    u8 alpha;
  } rgba;
  rgba8 ( void ) {};
  rgba8 ( u32 v ) { value = v; } ;
  rgba8 ( const union rgba8 & v ) { value = v.value; } ;
  union rgba8 & operator = ( const union rgba8 & v ) { value = v.value; return *this; }
} rgba8;

typedef union CAMUTILS_EXPORT rgb8
{
  u32 value;
  struct rgb
  {
    u8 blue;
    u8 green;
    u8 red;
  } rgb;
  rgb8 ( void ) {};
  rgb8 ( u32 v ) { value = v; } ;
  rgb8 ( const union rgb8 & v ) { value = v.value; } ;
  union rgb8 & operator = ( const union rgb8 & v ) { value = v.value; return *this; }
} rgb8;

typedef union CAMUTILS_EXPORT rgba16
{
  u64 value;
  struct rgba
  {
    u16 blue;
    u16 green;
    u16 red;
    u16 alpha;
  } rgba;
  rgba16 ( void ) {};
  rgba16 ( u64 v ) { value = v; } ;
  rgba16 ( const union rgba16 & v ) { value = v.value; } ;
  union rgba16 & operator = ( const union rgba16 & v ) { value = v.value; return *this; }
} rgba16;

}
}//end namespace arq

#endif // BASICTYPES_H
