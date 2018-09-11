#include "black_magic.h"
#include "log.h"
#include <map>
#include <cstring>
using namespace std;

#ifdef WIN32
int black_magic(serversock& t)
{
	return -1;
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

	map<vsock*, conn_data> mp;
	sock* ps = new sock;
	bool stop_server = false;
	while (!stop_server)
	{
		int ret = ep.wait(-1);
		if (ret <= 0)
		{
			loge("epoll error with ret: %d. errno: %d\n", ret, errno);
			break;
		}
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
						logd("Failed to finish accept.\n");
						delete ps;
					}
					else if (!accres.isSuccess())
					{
						logd("Accept is failed.\n");
						delete ps;
					}
					else
					{
						if (ps->setNonblocking() < 0)
						{
							logd("Failed to set client socket to non-blocking. %p\n", ps);
							delete ps;
						}
						else
						{
							logd("New connection accepted. Adding %p to epoll.\n", ps);
							if (ep.add(*ps, EPOLLIN | EPOLLET | EPOLLERR) < 0)
							{
								logd("Failed to adding to epoll. errno=%d\n", errno);
								delete ps;
							}
							else
							{
								// else, the socket is now added to epoll. So we don't release it.
								// prepare
								mp[(vsock*)ps].send_done = 0;
								mp[(vsock*)ps].binding = (void*)&ep;
								mp[(vsock*)ps].onNewData = request_handler_enter;
								mp[(vsock*)ps].onDataSent = response_handler_enter;
							}
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
					while (true)
					{
						memset(exbuff, 0, 10240);
						auto recres = s.recv_nb(exbuff, 10240);
						recres.setStopAtEdge(true);
						if (!recres.isFinished())
						{
							logd("Failed to finish recv. Removing from epoll and releasing resource... %p\n", &s);
							mp.erase(&s);
							ep.del(s);
							delete &s;
							break;
						}
						else if (!recres.isSuccess())
						{
							if (recres.getErrCode() == gerrno::WouldBlock)
							{
								// No more data yet
								mp[&v].recv_data.append(string(exbuff, recres.getBytesDone()));
								if (mp[&v].onNewData((sock&)v, mp[&v]))
								{
									logd("onNewData callback requires resource release. %p\n", &s);
									mp.erase(&v);
									ep.del(v);
									delete &s;
									break;
								}
							}
							else
							{
								// Recv call error.
								logd("Recv is Failed. Removing from epoll and releasing resource... %p\n", &s);
								mp.erase(&s);
								ep.del(s);
								delete &s;
								break;
							}
						}
						else
						{
							// Finished, Success
							// Store the data and loop again to read more. (until it reaches WouldBlock)
							// exbuff will be cleared at the beginning of the loop.
							mp[&s].recv_data.append(string(exbuff, recres.getBytesDone()));
						}
					}
				}
				else if (event& EPOLLOUT)
				{
					// Socket is writable (This is mostly cause by callbacks which set the EPOLLOUT)
					auto sndres = s.send_nb(mp[&v].send_data.data() + mp[&v].send_done, mp[&v].send_data.size() - mp[&v].send_done);
					sndres.setStopAtEdge(true);
					if (!sndres.isFinished())
					{
						logd("Failed to finish send. Removing from epoll and releasing resource... %p\n", &v);
						mp.erase(&s);
						ep.del(s);
						delete &s;
					}
					else if (!sndres.isSuccess())
					{
						if (sndres.getErrCode() == gerrno::WouldBlock)
						{
							// Can't send more data yet.
							mp[&v].send_done += sndres.getBytesDone();
							if (mp[&v].onDataSent((sock&)v, mp[&v]) < 0)
							{
								logd("onDataSent callback requires resource release. %p\n", &s);
								mp.erase(&s);
								ep.del(s);
								delete &s;
							}
						}
						else
						{
							// Send call error
							logd("Send is Failed. Removing from epoll and releasing resource... %p\n", &s);
							mp.erase(&s);
							ep.del(s);
							delete &s;
						}
					}
					else
					{
						mp[&v].send_done += sndres.getBytesDone();
						if (mp[&v].onDataSent((sock&)v, mp[&v]) < 0)
						{
							logd("onDataSent callback requires resource release. %p\n", &s);
							mp.erase(&s);
							ep.del(s);
							delete &s;
						}
					}
				}
				else if (event & EPOLLERR)
				{
					// Socket is error.
					loge("Socket is error. Removing from epoll and releasing resource... %p\n", &s);
					mp.erase(&s);
					ep.del(s);
					delete &s;
				}
			}
		});
	}

	return 0;
}
#endif