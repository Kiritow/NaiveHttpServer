#include "black_magic.h"
#include "log.h"
#include <map>
#include <cstring>
using namespace std;

#ifdef WIN32
int black_magic(serversock& t)
{
	return - 1;
}
#else
char exbuff[10240];
int black_magic(serversock& t)
{
	epoll ep(10240);
	ep.add(t, EPOLLIN);

	map<vsock*, string> mp;

	while (true)
	{
		int ret = ep.wait(-1);
		if (ret <= 0)
		{
			loge("epoll error with ret: %d. errno: %d\n", ret, errno);
			break;
		}
		// Handle events
		ep.handle([&](vsock& v, int event) {
			if (&v == &t)
			{
				// ServerSocket is readable. We can call accept() on it.
				sock* ps = new sock;
				if (t.accept(*ps) <= 0)
				{
					loge("Failed to accept.\n");
				}
				else
				{
					logd("Adding %p to epoll.\n", ps);
					ep.add(*ps, EPOLLIN);
				}
			}
			else
			{
				// Socket is readable. Read it.
				sock& s = (sock&)v;
				memset(exbuff, 0, 10240);
				int ret = s.recv(exbuff, 10240);
				if (ret <= 0)
				{
					loge("Failed to read. Removing from epoll.\n");
					ep.del(v, EPOLLIN);
				}
				else
				{
					mp[&v].append(string(exbuff, ret));
					if (isHttpHeader(mp[&v]))
					{
						request_handler_main(s,mp[&v]);
						logd("HTTP handle finished. removing from epoll...\n");
						ep.del(v, EPOLLIN);
						mp.erase(&v);
						delete &s;
					}
				}
			}
		});
	}

	return 0;
}
#endif