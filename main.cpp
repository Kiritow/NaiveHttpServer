#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "config.h"
#include "dirop.h"
#include "GSock/gsock.h"
#include "GSock/gsock_helper.h"
#include "NaiveThreadPool/ThreadPool.h"
#include "vmop.h"
#include "request.h"
#include "response.h"
#include "util.h"
#include "log.h"
#include "black_magic.h"
#include "get.h"
#include "post.h"
#include "parser.h"
using namespace std;

bool isHttpHeader(const std::string& header_raw)
{
	if (header_raw.find("\r\n\r\n") != std::string::npos)
	{
		return true;
	}
	else return false;
}

int recvheader_raw(sock& s,std::string& header_raw)
{
	std::string ans;
	while(true)
	{
		std::string tmp;
		int ret=recvline(s,tmp);
		if(ret<0) return ret;
		ans.append(tmp);
		if(isHttpHeader(ans))
		{
			break;
		}
	}
	header_raw=ans;
	return ans.size();
}

int request_unknown_handler(sock& s, const string& path, const string& version, const map<string, string>& mp)
{
	Response r;
	r.set_code(501);
	r.send_with(s);
	return 0;
}

int request_handler_main(sock& s,const std::string& header_raw)
{
	string method;
	string path;
	string version;
	map<string, string> mp;
	int ret = parse_header(header_raw, method, path, version, mp);
	logd("RequestHandler sock(%p): Header Parse Finished.\n", &s);
	if (ret < 0)
	{
		return -2;
	}

	logd("==========sock(%p)=========\nMethod: %s\nPath: %s\nVersion: %s\n", &s, method.c_str(), path.c_str(), version.c_str());
	for (auto& pr : mp)
	{
		logx(4, "%s\t %s\n", pr.first.c_str(), pr.second.c_str());
	}
	logd("^^^^^^^^^^sock(%p)^^^^^^^^^^\n", &s);
	logd("RequestHandler sock(%p): Finished Successfully.\n", &s);

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

int request_handler(sock& s)
{
	logd("RequestHandler sock(%p): Started\n", &s);
	string peer_ip;
	int peer_port;
	if (s.getpeer(peer_ip, peer_port) < 0)
	{
		logd("RequestHandler sock(%p): Failed to get peer info. This is not an error.\n",&s);
	}
	else
	{
		logd("RequestHandler sock(%p): Connected From %s:%d\n", &s, peer_ip.c_str(), peer_port);
	}

	string header_raw;
	int ret=recvheader_raw(s,header_raw);
	if(ret<0)
	{
		return -1;
	}
	logd("RequestHandler sock(%p): Header Received.\n", &s);
	
	return request_handler_main(s, header_raw);
}

void bad_request_handler(sock& s)
{
	Response r;
	r.set_code(400);
	r.send_with(s);
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
		_server_port = 8000;
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
		int ret=black_magic(t);
		if (ret == 0)
		{
			logi("Server closed from rapid mode.\n");
			return 0;
		}
		else
		{
			loge("Failed to enter rapid mode.\n");
			return 0;
		}
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
					int ret=request_handler(*ps);
					logd("request handler returns %d\n",ret);
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

