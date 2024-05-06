#ifndef GOCOROUTINE_UTILS_H
#define GOCOROUTINE_UTILS_H

#if __cplusplus >= 202002L

#define __cpp_lib_coroutine
#include <coroutine>

#endif


#if __cplusplus >= 201703L

#define FMTLOG_HEADER_ONLY
#include "fmtlog/fmtlog.h"

#endif

#include <memory>


#define GOCOROUTINE_NAMESPACE_BEGIN namespace gocoroutine {

#define GOCOROUTINE_NAMESPACE_END }      /* end of namespace gocoroutine */

GOCOROUTINE_NAMESPACE_BEGIN

#define SECONDS(x) (int64_t(x) * 1000 * 1000 * 1000) // s->ns
#define MILLISECONDS(x) (int64_t(x) * 1000 * 1000)   // ms->ns
#define MICROSECONDS(x) (int64_t(x) * 1000)          // us->ns

#if __cplusplus >= 201703L

#define SETLOGLEVEL(level) fmtlog::setLogLevel(level)
#define SETLOGHEADER(header) fmtlog::setHeaderPattern(header)

#define DEBUGFMTLOG(str, ...) logd(str, ##__VA_ARGS__)
#define INFOFMTLOG(str, ...) logi(str, ##__VA_ARGS__)
#define WARNINGFMTLOG(str, ...) logw(str, ##__VA_ARGS__)
#define ERRORFMTLOG(str, ...) loge(str, ##__VA_ARGS__)

#define DEBUGFMTLOG_ONCE(str, ...) logdo(str, ##__VA_ARGS__)
#define INFOFMTLOG_ONCE(str, ...) logio(str, ##__VA_ARGS__)
#define WARNINGFMTLOG_ONCE(str, ...) logwo(str, ##__VA_ARGS__)
#define ERRORFMTLOG_ONCE(str, ...) logeo(str, ##__VA_ARGS__)

#define FMTLOGPOLL() fmtlog::poll()
#define CREATEPOLLTHREAD(t) fmtlog::startPollingThread(t)

#define SETLOGFILE(file, flag) fmtlog::setLogFile(file, flag)
#define CLOSELOGFILE() fmtlog::closeLogFile()

#else

#define SETLOGLEVEL(level) ;
#define SETLOGHEADER(header) ;

#define DEBUGFMTLOG(str, ...) ;
#define INFOFMTLOG(str, ...) ;
#define WARNINGFMTLOG(str, ...) ;
#define ERRORFMTLOG(str, ...) ;

#define DEBUGFMTLOG_ONCE(str, ...) ;
#define INFOFMTLOG_ONCE(str, ...) ;
#define WARNINGFMTLOG_ONCE(str, ...) ;
#define ERRORFMTLOG_ONCE(str, ...) ;

#define POLL() ;
#define CREATEPOLLTHREAD(t) ;

#define SETLOGFILE(file, flag) ;
#define CLOSELOGFILE() ;

#endif

#if __cplusplus >= 201402L

#define MAKE_UNIQUE(T, args...) \
    std::make_unique<T>(args)

#define MAKE_UNIQUE_ARRAY(T, size) \
    std::make_unique<T[]>(size)

#else

#define MAKE_UNIQUE(T, args...) \
    std::unique_ptr<T>(new T(args))

#define MAKE_UNIQUE_ARRAY(T, size) \
    std::unique_ptr<T[]>(new T[size])

#endif

#define likely(x) __builtin_expect(!!(x), 1) 
#define unlikely(x) __builtin_expect(!!(x), 0)

GOCOROUTINE_NAMESPACE_END

#endif  