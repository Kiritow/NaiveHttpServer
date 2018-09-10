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
	if (t.setNonblocking() < 0)
	{
		loge("Failed to set serversocket to non-blocking\n");
		return -1;
	}

	epoll ep(10240);
	ep.add(t, EPOLLIN | EPOLLET | EPOLLERR);

	map<vsock*, string> mp;

	while (true)
	{
		int ret = ep.wait(-1);
		if (ret <= 0)
		{
			loge("epoll error with ret: %d. errno: %d\n", ret, errno);
			break;
		}
		bool stop_server = false;
		// Handle events
		ep.handle([&](vsock& v, int event) {
			logd("epoll handle: vsock %p event %d\n", &v, event);
			if (&v == &t)
			{
				if (event & EPOLLIN)
				{
					// ServerSocket is readable. We can call accept() on it.
					sock* ps = new sock;
					auto accres = t.accept_nb(*ps);
					if (!accres.isFinished())
					{
						loge("Failed to finish accept.\n");
					}
					else if (!accres.isSuccess())
					{
						loge("Accept is failed.\n");
					}
					else
					{
						if (ps->setNonblocking() < 0)
						{
							loge("Failed to set client socket to non-blocking. %p\n", ps);
							delete ps;
						}
						else
						{
							logd("New connection accepted. Adding %p to epoll.\n", ps);
							ep.add(*ps, EPOLLIN | EPOLLET | EPOLLERR);
						}
					}
				}
				else if (event & EPOLLERR)
				{
					// Server socket is error. Stop Server.
					stop_server = true;
				}
			}
			else
			{
				sock& s = (sock&)v;
				if (event & EPOLLIN)
				{
					// Socket is readable. Read it
					memset(exbuff, 0, 10240);
					auto recres = s.recv_nb(exbuff, 10240);
					recres.setStopAtEdge(true);
					if (!recres.isFinished())
					{
						loge("Failed to finish recv. Removing from epoll and releasing resource... %p\n", &s);
						mp.erase(&s);
						ep.del(s, EPOLLIN | EPOLLET | EPOLLERR);
						delete &s;
					}
					else if (!recres.isSuccess())
					{
						if (recres.getErrCode() == gerrno::WouldBlock)
						{
							// No more data yet
							mp[&s].append(string(exbuff, recres.getBytesDone()));
							if (isHttpHeader(mp[&s]))
							{
								// This is http header.
								request_handler_main(s, mp[&s]);
								logd("HTTP handle finished. removing from epoll and releasing resource... %p\n", &s);
								mp.erase(&s);
								ep.del(s, EPOLLIN | EPOLLET | EPOLLERR);
								delete &s;
							}
						}
						else
						{
							loge("Recv is Failed. Removing from epoll and releasing resource... %p\n", &s);
							mp.erase(&s);
							ep.del(s, EPOLLIN | EPOLLET | EPOLLERR);
							delete &s;
						}
					}
					else
					{
						// Here is a little bug, if request header is more than 10240 and mode is ET, then we will infinitely hang up here.
						// Finished, Success
						mp[&s].append(string(exbuff, recres.getBytesDone()));
						if (isHttpHeader(mp[&s]))
						{
							// This is http header.
							request_handler_main(s, mp[&s]);
							logd("HTTP handle finished. removing from epoll and releasing resource... %p\n", &s);
							ep.del(s, EPOLLIN | EPOLLET | EPOLLERR);
							mp.erase(&s);
							delete &s;
						}
					}
				}
				else if (event & EPOLLERR)
				{
					// Socket is error.
					loge("Socket is error. Removing from epoll and releasing resource... %p\n", &s);
					mp.erase(&s);
					ep.del(s, EPOLLIN | EPOLLET | EPOLLERR);
				}
			}
		});
	}

	return 0;
}
#endif