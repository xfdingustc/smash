#ifndef __H_Linux__
#define __H_Linux__


#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/time.h>

#include <sys/time.h>
#include <basetypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define _DEBUG

typedef unsigned char       UINT8;  /**< UNSIGNED 8-bit data type */
typedef unsigned short      UINT16; /**< UNSIGNED 16-bit data type */
typedef unsigned int        UINT32; /**< UNSIGNED 32-bit data type */
typedef unsigned long long  UINT64; /**< UNSIGNED 64-bit data type */
typedef unsigned short      WORD;   /**< UNSIGNED 64-bit data type */
typedef unsigned int        DWORD;  /**< UNSIGNED 64-bit data type */
typedef unsigned char       BYTE;   /**< UNSIGNED 8-bit data type */
typedef long                LONG;
typedef long long           LONGLONG; /**< UNSIGNED 64-bit data type */
typedef long                INT32;

#endif
