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
struct vpack
{
	string send_data;
	int sent;

	string recv_data;

	// 0 Waiting for header
	// 1 Received complete header, POST data not checked.
	// 2 Received complete header, receiving post data...
	// 3 Received complete header, all data ready (or the request is GET)
	// 4 Request is handled. Sending data...
	// 5 About to be released.
	int status;

	Request req;
	size_t header_endpos;
	int post_total;
};

int black_magic(serversock& t)
{
	if (t.setNonblocking() < 0)
	{
		loge("Failed to set serversocket to non-blocking\n");
		return -1;
	}

	epoll ep(10240);
	ep.add(t, EPOLLIN | EPOLLET | EPOLLERR);

	map<vsock*,vpack> mp;
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
								// Initialize vairables
								mp[(vsock*)ps].sent = 0;
								mp[(vsock*)ps].status = 0;
							}
						}
					}
				}
				else if (event & EPOLLERR)
				{
					// Server socket is error. Stop Server.
					loge("EPOLLERR on serversock. Stopping server...\n");
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
							vpack& thispack = mp[&s];

							if (recres.getErrCode() == gerrno::WouldBlock)
							{
								// No more data yet
								thispack.recv_data.append(string(exbuff, recres.getBytesDone()));

								if (thispack.status == 0)
								{
									// Check if it contains http request header
									if (string::npos != (thispack.header_endpos = thispack.recv_data.find("\r\n\r\n")))
									{
										int ret = parse_header(thispack.recv_data, thispack.req);
										if (ret < 0)
										{
											thispack.status = 5;
											logd("failed to parse http header. ret=%d. status switched to 5\n", ret);
										}
										else
										{
											thispack.status = 1;
											logd("http header received and parsed. status switched to 1.");
										}
									}
								}

								if (thispack.status == 2)
								{
									// check if we have done receiving post data.
									if (thispack.post_total <= thispack.req.data.size())
									{
										thispack.status = 3;
										logd("http post data received. status switched to 3.\n");
									}
								}

								if (thispack.status == 1)
								{
									// Check if it needs more data
									if (thispack.req.method == "POST")
									{
										auto iter = thispack.req.header.find("Content-Length");
										int content_length = 0;
										if (iter != thispack.req.header.end() && sscanf(iter->second.c_str(), "%d", &content_length) == 1)
										{
											// More data is need to read.
											// First check if some posted data is already in str
											if (thispack.header_endpos + 4 != thispack.recv_data.size())
											{
												// Some posted data here
												thispack.req.data = thispack.recv_data.substr(thispack.header_endpos + 4);
											}
											thispack.recv_data.clear();
											thispack.post_total = content_length;
											logd("more post data is need. Switch status to 2.\n");
										}
										else
										{
											thispack.status = 5;
											logd("invalid post header without Content-Length. status switched to 5.\n");
										}
									}
									else
									{
										thispack.status = 3;
										logd("Not a POST request. status switched to 3.\n");
									}
								}

								if (thispack.status == 3)
								{
									Response res;
									int ret = request_handler(thispack.req, res);
									if (ret < 0)
									{
										res.set_code(400);
									}

									thispack.send_data = res.toString();
									thispack.sent = 0;
									thispack.status = 4;
									logd("Request handled. status switch to 4.\n");

									// Try send it
									NBSendResult sendres = s.send_nb(thispack.send_data.c_str(), thispack.send_data.size());
									sendres.setStopAtEdge(true);
									if (!sendres.isFinished())
									{
										// If it cannot stop at edge, it might be something is wrong.
										thispack.status = 5;
										logd("Failed to finish send. status switch to 5.\n");
									}
									else if (!sendres.isSuccess())
									{
										if (sendres.getErrCode() == gerrno::WouldBlock)
										{
											// If we meet WouldBlock, add EPOLLOUT on it.
											// Then we keep status at 4.
											// We will meet again in EPOLLOUT brench when this socket is writable again.
											ep.mod(v, EPOLLOUT);
											thispack.sent = sendres.getBytesDone();
										}
										else
										{
											thispack.status = 5;
											logd("Send is Failed. errno=%d. status switched to 5.\n", sendres.getErrCode());
										}
									}
									else
									{
										thispack.status = 5;
										logd("Response send finished immediately. status switch to 5.\n");
									}
								}

								if (thispack.status == 5)
								{
									logd("vpack with status 5. Removing it from epoll and releasing resouce...\n");
									// After this line, thispack is invalid an should never be used again.
									mp.erase(&s);
									ep.del(s);
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
					// Socket is writable (Oh it's you! we meet again here. But it would be a short time.)
					auto sndres = s.send_nb(mp[&v].send_data.data() + mp[&v].sent, mp[&v].send_data.size() - mp[&v].sent);
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
							mp[&v].sent += sndres.getBytesDone();
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
						mp[&v].sent += sndres.getBytesDone();
						// All data is sent!
						logd("Response send finished. Cleaning up...\n");
						mp.erase(&s);
						ep.del(s);
						delete &s;
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