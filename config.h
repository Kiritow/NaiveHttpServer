#pragma once

#define BIND_PORT 8000
#define SERVER_ROOT std::string(".")

#ifdef HCDEBUG
#define dprintf(fmt,...) printf(fmt,__VA_ARGS__)
#else
#define dprintf(fmt,...)
#endif