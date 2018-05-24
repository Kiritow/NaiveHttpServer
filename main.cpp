#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "config.h"
#include "dirop.h"
#include "GSock/gsock.h"
#include "NaiveThreadPool/ThreadPool.h"
#include "vmop.h"
#include "request.h"
#include "response.h"
#include "util.h"

using namespace std;

int recvheader_raw(sock& s,std::string& header_raw)
{
	std::string ans;
	while(true)
	{
		std::string tmp;
		int ret=recvline(s,tmp);
		if(ret<0) return ret;
		ans.append(tmp);
		if(ans.find("\r\n\r\n")!=std::string::npos)
		{
			break;
		}
	}
	header_raw=ans;
	return ans.size();
}

int GetFileContent(const string& request_path, string& out_content)
{
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	char buff[1024];
	string content;
	while (true)
	{
		memset(buff, 0, 1024);
		int ret = fread(buff, 1, 1024, fp);
		if (ret <= 0)
		{
			break;
		}
		content.append(string(buff, ret));
	}
	fclose(fp);
	out_content = content;
	return 0;
}

#define ct(abbr,target) else if(endwith(path,abbr)) out_content_type=target

int GetFileContentType(const string& path, string& out_content_type)
{
	if (endwith(path, ".html"))
	{
		out_content_type = "text/html";
	}
	ct(".bmp", "application/x-bmp");
	ct(".doc", "application/msword");
	ct(".ico", "image/x-icon");
	ct(".java", "java/*");
	ct(".class", "java/*");
	ct(".jpeg", "image/jpeg");
	ct(".jpg", "image/jpeg");
	ct(".png", "image/png");
	ct(".swf", "application/x-shockwave-flash");
	ct(".xhtml", "text/html");
	ct(".apk", "application/vnd.android.package-archive");
	ct(".exe", "application/x-msdownload");
	ct(".htm", "text/html");
	ct(".js", "application/x-javascript");
	ct(".mp3", "audio/mp3");
	ct(".mp4", "video/mpeg4");
	ct(".mpg", "video/mpg");
	ct(".pdf", "application/pdf");
	ct(".rmvb", "application/vnd.rn-realmedia-vbr");
	ct(".torrent", "application/x-bittorrent");
	ct(".txt", "text/plain");
	else
	{
		/// Not Support Content Type
		return -1;
	}

	/// Supported ContentType
	return 0;
}

#undef ct

int request_get_handler(sock& s, const string& in_path, const string& version, const map<string, string>& mp)
{
	// URL Decode first
	string path;
	if (urldecode(in_path, path) < 0)
	{
		printf("Failed to decode url : %s\n", in_path.c_str());
		return -1;
	}

	// Request to / could be dispatched to /index.html,/index.lua
	if (endwith(path, "/"))
	{
		if (request_get_handler(s, path + "index.html", version, mp) < 0 &&
			request_get_handler(s, path + "index.lua", version, mp) < 0)
		{
			// Display a list
			string ans;

			ans.append("<html><head><title>Index of " + path + "</title></head><body><h1>Index of " + path + "</h1>");
			string real_dir = SERVER_ROOT + path;
			printf("About to list Directory: %s\n", real_dir.c_str());
			DirWalk w(real_dir);
			string filename;
			int is_dir;
			while (w.next(filename, is_dir) > 0)
			{
				string realname;
				urlencode(filename, realname);

				ans.append("<p><a href='" + realname);
				if (is_dir)
				{
					ans.append("/");
				}
				ans.append("'>" + filename + "</a>");
				if (is_dir)
				{
					ans.append(" [dir]");
				}
				ans.append("</p>");
			}
			ans.append("</body></html>");

			Response r;
			r.set_code(200);
			r.setContent(ans);
			r.send_with(s);
			return 0;
		}
		else
		{
			return 0;
		}
	}

	int request_type = get_request_type(path);
	if (request_type < 0)
	{
		// Invalid request
		return -1;
	}

	if (request_type == 0)
	{
		// Static Target
		// Just read out and send it.
		string content;
		if (GetFileContent(path, content) < 0)
		{
			/// File not readable.
			Response r;
			r.set_code(500);
			r.send_with(s);
			return 0;
		}
		else
		{
			Response r;
			r.set_code(200);
			string content_type;
			if (GetFileContentType(path, content_type) < 0) content_type = "text/plain";
			r.setContent(content, content_type);
			r.send_with(s);
			return 0;
		}
	}
	else
	{
		// Dynamic Target
		// Currently not supported.
		Response r;
		r.set_code(501);
		r.send_with(s);
		return 0;
	}
}

int request_post_handler(sock& s, const string& path, const string& version, const map<string, string>& mp)
{
	int request_type = get_request_type(path);
	if (request_type < 0)
	{
		Response r;
		r.set_code(404);
		r.send_with(s);
		return 0;
	}
	
	if (request_type == 0)
	{
		// Static Target
		// POST on static target is not allowed.
		Response r;
		r.set_code(405);
		r.send_with(s);
		return 0;
	}
	else
	{
		// Dynamic Target
		// Currently not supported.
		Response r;
		r.set_code(501);
		r.send_with(s);
		return 0;
	}
}

int request_unknown_handler(sock& s, const string& path, const string& version, const map<string, string>& mp)
{
	Response r;
	r.set_code(501);
	r.send_with(s);
	return 0;
}

int request_handler(sock& s)
{
	printf("RequestHandler sock(%p): Started\n", &s);
	string header_raw;
	int ret=recvheader_raw(s,header_raw);
	if(ret<0)
	{
		return -1;
	}
	printf("RequestHandler sock(%p): Header Received.\n", &s);
	string method;
	string path;
	string version;
	map<string, string> mp;
	ret = parse_header(header_raw, method, path, version, mp);
	printf("RequestHandler sock(%p): Header Parse Finished.\n", &s);
	if (ret < 0)
	{
		return -2;
	}

	printf("==========sock(%p)=========\nMethod: %s\nPath: %s\nVersion: %s\n", &s, method.c_str(), path.c_str(), version.c_str());
	for (auto& pr : mp)
	{
		printf("%s\t %s\n", pr.first.c_str(), pr.second.c_str());
	}
	printf("^^^^^^^^^^sock(%p)^^^^^^^^^^\n", &s);
	printf("RequestHandler sock(%p): Finished Successfully.\n", &s);

	if (method == "GET")
	{
		if (request_get_handler(s, path, version, mp) < 0)
		{
			return 1;
		}
	}
	else if (method == "POST")
	{
		if (request_post_handler(s, path, version, mp) < 0)
		{
			return 2;
		}
	}
	else
	{
		if (request_unknown_handler(s, path, version, mp) < 0)
		{
			return 3;
		}
	}
	
	return 0;
}

void bad_request_handler(sock& s)
{
	Response r;
	r.set_code(400);
	r.send_with(s);
}

void testMain()
{
	VM v;
	v.runCode("print('Hello World')");
	v.runCode("response['content_type']='text/html'");

	v.runCode("print(response.content_type)");

	exit(0);
}

int main()
{
	dprintf("NaiveHTTPServer Started.\n");
	serversock t;
	if (t.set_reuse() < 0)
	{
		dprintf("Failed to set reuse flag. This is not an error.\n");
	}
	if(t.bind(BIND_PORT)<0)
	{
		dprintf("Failed to bind at port %d\n",BIND_PORT);
		return 0;
	}
	if(t.listen(10)<0)
	{
		dprintf("Failed to listen at port %d\n",BIND_PORT);
	}
	dprintf("Server started at port %d\n",BIND_PORT);
	dprintf("Starting thread pool...\n");
	ThreadPool tp(10);
	dprintf("Server is now ready for connections.\n");
	while(true)
	{
		sock* ps=new sock;
		int ret=t.accept(*ps);
		if(ret<0)
		{
			dprintf("Failed to accept connection. Abort.\n");
			break;
		}
		if(tp.start([ps](){
					int ret=request_handler(*ps);
					dprintf("request handler returns %d\n",ret);
					if (ret < 0)
					{
						bad_request_handler(*ps);
					}
                    else if(ret>0)
                    {
                        // 404 if ret>0
                        Response r;
                        r.set_code(404);
                        r.send_with(*ps);
                    }
					delete ps;
				})<0)
		{
			dprintf("Failed to start job at thread pool.\n");
		}
		else
		{
			dprintf("Job started with sock: %p\n",ps);
		}
	}
	dprintf("Server closed.\n");

	return 0;
}

