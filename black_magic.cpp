#include "black_magic.h"

#include "log.h"

#ifdef WIN32
int black_magic(serversock& t)
{
	return - 1;
}
#else
struct epoll_event events[1024];
int black_magic(serversock& t)
{
	epoll ep;
	ep.add(t, EPOLLIN);

	while (true)
	{
		int ret = ep.wait(events, 1024, -1);
		if (ret <= 0)
		{
			loge("epoll error with ret: %d. errno: %d\n", ret, errno);
			break;
		}
		// Handle events
		for (int i = 0; i < ret; i++)
		{
			
		}
	}

	return 0;
}
#endif