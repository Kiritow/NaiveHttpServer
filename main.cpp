#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "config.h"
#include "dirop.h"
#include "GSock/gsock.h"
#include "GSock/gsock_helper.h"
#include "NaiveThreadPool/ThreadPool.h"
#include "vmop.h"
#include "log.h"
#include "util.h"
#include "black_magic.h"
#include "get.h"
#include "post.h"
using namespace std;

void request_handler_unknown(const Request& req, Response& res)
{
	res.set_code(501);
}

// Returns:
// -1 Request is not ready. (This is an error)
// 0 Request is handled successfully.
// 1 Failed to handle GET request. (Error)
// 2 Failed to handle POST request. (Error)
// 3 POST request need more data. (Not Error)
// 4 Unsupported http method. (Error)
int request_handler(const Request& req,Response& res)
{
	logd("==========request(%p)=========\nMethod: %s\nPath: %s\nVersion: %s\n", 
		&req, req.method.c_str(), req.path.c_str(), req.http_version.c_str());
	for (auto& pr : req.header)
	{
		logx(4, "%s\t %s\n", pr.first.c_str(), pr.second.c_str());
	}
	logd("^^^^^^^^^^request(%p)^^^^^^^^^^\n", &req);

	if (req.method == "GET")
	{
		if (request_handler_get(req,res) < 0)
		{
			return -1;
		}
	}
	else if (req.method == "POST")
	{
		if (request_handler_post(req, res) < 0)
		{
			return -2;
		}
	}
	else
	{
		request_handler_unknown(req, res);
	}

	return 0;
}

// Used in blocked socket (Normal mode)
// Returns:
// 0:  OK
// -1: socket read failed.
// -2: Invalid header
// -3: Post without content length
int receive_request(sock& s,Request& req)
{
	string str;
	char buff[1024];
	size_t endpos;
	while (true)
	{
		int ret = s.recv(buff, 1024);
		if (ret <= 0) return -1;
		str.append(string(buff, ret));
		if (string::npos != (endpos = str.find("\r\n\r\n")))
		{
			break;
		}
	}
	int ret = parse_header(str, req);
	if (ret < 0) return -2;
	if (req.method == "POST")
	{
		auto iter = req.header.find("Content-Length");
		int content_length = 0;
		if (iter != req.header.end() && sscanf(iter->second.c_str(), "%d", &content_length) == 1)
		{
			// Try to receive posted data.
			// First check if some posted data is already in str
			if (endpos + 4 != str.size())
			{
				// Some posted data here
				req.data = str.substr(endpos + 4);
			}

			int done = 0;
			while (done < content_length)
			{
				int ret = s.recv(buff, std::min(1024, content_length - done));
				if (ret <= 0) return -1;
				req.data.append(string(buff, ret));
				done += ret;
			}
		}
		else return -3;
	}

	return 0;
}

// Used in blocked socket (Normal mode)
void send_response(sock& s, Response& res)
{
	string str = res.toString();
	sock_helper sp(s);
	sp.sendall(str);
}

int _server_port;
string _server_root;
int _deploy_mode;
const int& _get_bind_port()
{
	return _server_port;
}
const string& _get_server_root()
{
	return _server_root;
}
const int& _get_deploy_mode()
{
	return _deploy_mode;
}

int read_config()
{
	// read config.lua 
	string content;
	if (GetFileContent("config.lua", content) < 0)
	{
		_server_port = 9001;
		_server_root = ".";
		logd("Configure file not found. Fallback to default.\n");
		return 0;
	}

	VM v;

	// The config.lua should set the following variable:
	// server_port = ... (a number)
	// server_root = ... (a string)
	if (v.runCode(content) < 0)
	{
		// Failed to run config.lua
		return -1;
	}

	lua_State* L = v.get();
	lua_getglobal(L, "deploy_mode");
	lua_getglobal(L, "server_root");
	lua_getglobal(L, "server_port");
	if (!lua_isinteger(L, -1))
	{
		loge("server_port is not integer");
		return -1;
	}
	_server_port = lua_tointeger(L, -1);
	if (!lua_isstring(L, -2))
	{
		loge("server_root is not string");
		return -2;
	}
	_server_root = lua_tostring(L, -2);
	if (!lua_isinteger(L, -3))
	{
		loge("deploy_mode is not integer");
		return -3;
	}
	_deploy_mode = lua_tointeger(L, -3);

	logd("Read from configure file:\nServerRoot: %s\nBindPort: %d\nDeploy Mode: %d\n", _server_root.c_str(), _server_port,_deploy_mode);
	return 0;
}

int main()
{
	logi("NaiveHTTPServer Started.\n");
	if (read_config() < 0)
	{
		loge("Failed to read configure. Fatal error.\n");
		return 0;
	}

	serversock t;
	if (t.set_reuse() < 0)
	{
		logw("Failed to set reuse flag. This is not an error.\n");
	}
	if(t.bind(BIND_PORT)<0)
	{
		loge("Failed to bind at port %d\n",BIND_PORT);
		return 0;
	}
	if(t.listen(1024)<0)
	{
		loge("Failed to listen at port %d\n",BIND_PORT);
		return 0;
	}
	logi("Server started at port %d\n",BIND_PORT);
	logi("Server root is %s\n", SERVER_ROOT.c_str());

	if (DEPLOY_MODE != 0)
	{
		logi("Entering rapid mode, black magic started.\n");
		int ret = black_magic(t);
		if (ret == 0)
		{
			logi("Server closed from rapid mode.\n");
		}
		else
		{
			loge("Failed to enter rapid mode.\n");
		}

		return 0;
	}

	logi("Starting thread pool...\n");
	ThreadPool tp(10);
	logi("Server is now ready for connections.\n");
	while(true)
	{
		sock* ps=new sock;
		int ret=t.accept(*ps);
		if(ret<0)
		{
			loge("Failed to accept connection. Abort.\n");
			break;
		}
		if(tp.start([ps](){
			logd("receving request on sock %p\n", ps);
			Request req;
			int ret = receive_request(*ps, req);
			if (ret < 0)
			{
				logd("Failed to receive request on sock %p\n", ps);
			}
			else
			{
				Response res;
				ret = request_handler(req, res);
				if (ret < 0)
				{
					res.set_code(400);
				}
				send_response(*ps, res);
			}
			delete ps;
		})<0)
		{
			logw("Failed to start job at thread pool.\n");
		}
		else
		{
			logd("Job started with sock: %p\n",ps);
		}
	}
	logi("Server closed.\n");

	return 0;
}

