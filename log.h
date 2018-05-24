#pragma once

// 0 None (Silent)
// 1 Error
// 2 Error, Warning
// 3 Error, Warning, Info
// 4 Error, Warning, Info, Debug

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL 4
#endif

// Log Helper
std::string _log_whichThread();

#define _RealLog(level,fmt,...) do{std::string _currentThread=_log_whichThread(); printf("<%s|%s|%s> " fmt,level,_currentThread.c_str(),__func__,##__VA_ARGS__);}while(0)

#if (CONFIG_LOG_LEVEL==0)
#define loge(fmt,...)
#define logw(fmt,...)
#define logi(fmt,...)
#define logd(fmt,...)
#elif (CONFIG_LOG_LEVEL==1)
#define loge(fmt,...) _RealLog("Error",fmt,##__VA_ARGS__)
#define logw(fmt,...)
#define logi(fmt,...)
#define logd(fmt,...)
#elif (CONFIG_LOG_LEVEL==2)
#define loge(fmt,...) _RealLog("Error",fmt,##__VA_ARGS__)
#define logw(fmt,...) _RealLog("Warn",fmt,##__VA_ARGS__)
#define logi(fmt,...)
#define logd(fmt,...)
#elif (CONFIG_LOG_LEVEL==3)
#define loge(fmt,...) _RealLog("Error",fmt,##__VA_ARGS__)
#define logw(fmt,...) _RealLog("Warn",fmt,##__VA_ARGS__)
#define logi(fmt,...) _RealLog("Info",fmt,##__VA_ARGS__)
#define logd(fmt,...)
#else
#define loge(fmt,...) _RealLog("Error",fmt,##__VA_ARGS__)
#define logw(fmt,...) _RealLog("Warn",fmt,##__VA_ARGS__)
#define logi(fmt,...) _RealLog("Info",fmt,##__VA_ARGS__)
#define logd(fmt,...) _RealLog("Debug",fmt,##__VA_ARGS__)
#endif

// logx is for special use. For example, print multiple lines of debug info.
#if (CONFIG_LOG_LEVEL>0)
#define logx(level,fmt,...) do{if(level>0&&level<=CONFIG_LOG_LEVEL) printf(fmt,##__VA_ARGS__);}while(0)
#else
#define logx(fmt,...)
#endif
