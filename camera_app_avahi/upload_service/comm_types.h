/*==============================================================================
FILE    NAME    :   comm_types.h

FILE    DESC    :   comm types define 

AUTHOR          :   wangxutao

DATE            :   20140627
===============================================================================*/

#ifndef  __COMM_TYPES_H_20140627
#define  __COMM_TYPES_H_20140627 1


//unified platform: support win32 linux32 linux64
#ifdef WIN32_OS
typedef int		            sint32;
typedef unsigned int	    uint32;
typedef __int64		        sint64;
typedef unsigned __int64    uint64;
#else
#include <stdint.h>
typedef  int32_t        sint32;
typedef  uint32_t       uint32;
typedef  int64_t 		sint64;
typedef  uint64_t		uint64;
#endif 

typedef short	sint16;
typedef unsigned short uint16;
typedef char sint8;
typedef unsigned char uint8;
typedef  unsigned char      uchar;

#endif //__COMM_TYPES_H_20140627

