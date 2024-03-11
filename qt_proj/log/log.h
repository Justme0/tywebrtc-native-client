#pragma once

#include <QtGlobal>

#include "tylib/time/time_util.h"
#include "tylib/time/timer.h"

// detect OS https://stackoverflow.com/a/34166706/1204713
#if defined(Q_OS_WIN)
#define STRIP_FILENAME(x) strrchr(x, '\\') ? strrchr(x, '\\') + 1 : x
#else
#define STRIP_FILENAME(x) strrchr(x, '/') ? strrchr(x, '/') + 1 : x
#endif

#define tylog(format, arg...)                                                \
  qDebug("%s %s:%d %s() " format,                                            \
         (g_now.ComputeNow(),                                                \
          tylib::MicroSecondToLocalTimeString(g_now.MicroSeconds()).data()), \
         STRIP_FILENAME(__FILE__), __LINE__, __FUNCTION__, ##arg)
