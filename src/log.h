/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <errno.h>
#include <string.h>
#include <utils/Log.h>

#define _ERRNO_STR(_func, _err) \
  "%s failed: %s", (_func), strerror(_err)

#define ALOGE_ERRNO_NO(_func, _err) \
  ALOGE(_ERRNO_STR(_func, _err))

#define ALOGE_ERRNO(_func) \
  ALOGE_ERRNO_NO(_func, errno)

#define ALOGW_ERRNO_NO(_func, _err) \
  ALOGW(_ERRNO_STR(_func, _err))

#define ALOGW_ERRNO(_func) \
  ALOGW_ERRNO_NO(_func, errno)
