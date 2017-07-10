/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef _CAM_LOG_H_
#define _CAM_LOG_H_

#include "ykcamera_common.h"

CXX_EXTERN_BEGIN
/* CAM_log API */

#define CAM_LOG_QUIET    -8

/**
 * Something went really wrong and we will crash now.
 */
#define CAM_LOG_PANIC     0

/**
 * Something went wrong and recovery is not possible.
 * For example, no header was found for a format which depends
 * on headers or an illegal combination of parameters is used.
 */
#define CAM_LOG_FATAL     8

/**
 * Something went wrong and cannot losslessly be recovered.
 * However, not all future data is affected.
 */
#define CAM_LOG_ERROR    16

/**
 * Something somehow does not look correct. This may or may not
 * lead to problems. An example would be the use of '-vstrict -2'.
 */
#define CAM_LOG_WARNING  24

#define CAM_LOG_INFO     32
#define CAM_LOG_VERBOSE  40

/**
 * Stuff which is only useful for  developers.
 */
#define CAM_LOG_DEBUG    48

#define CAM_LOG_MAX_OFFSET (CAM_LOG_DEBUG - CAM_LOG_QUIET)
	int cam_log_init(int log_level);
	void clogd(const char *fmt, ...);
	void cam_log(int level, const char *tag, const char *log);
	void cam_log_debug(const char *tag, const char *log);
	void cam_log_printf(int level, const char *tag, const char *fmt, ...);

 CXX_EXTERN_END
#endif
