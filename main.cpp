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

int GetFileLength(const string& request_path, int& out_length)
{
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	fseek(fp, 0L, SEEK_END);
	out_length = ftell(fp);
	fclose(fp);
	return 0;
}

int GetFileContentEx(const string& request_path, int beginat, int length, string& out_content)
{
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	fseek(fp, beginat, SEEK_SET);
	char buff[1024];
	string content;
	int done = 0;
	int total = length;
	while (done < total)
	{
		memset(buff, 0, 1024);
		int ret = fread(buff, 1, mymin(1024,total-done), fp);
		if (ret <= 0)
		{
			break;
		}
		content.append(string(buff, ret));
		done += ret;
	}
	fclose(fp);
	out_content = content;
	return 0;
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

// Read and directly send
int ReadFileAndSend(Response& req, sock& s, const string& request_path)
{
	req.send_with(s);
	sock_helper sp(s);
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	char buff[10240];
	string content;
	while (true)
	{
		int ret = fread(buff, 1, 10240, fp);
		if (ret <= 0)
		{
			break;
		}
		sp.sendall(buff, ret);
	}
	fclose(fp);
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

int request_get_dynamic_handler(sock& s, const string& path_decoded, const string& version, const map<string, string>& mp, const map<string,string>& param)
{
	logd("Loading lua file: %s\n", path_decoded.c_str());

	string lua_code;
	int ret = GetFileContent(path_decoded, lua_code);
	if (ret < 0)
	{
		return -1;
	}

	VM v;
	auto L = v.get();
	lua_newtable(L);
	lua_pushstring(L, version.c_str());
	lua_setfield(L, 1, "http_version"); // request["http_version"]=...

	lua_pushstring(L, "GET");
	lua_setfield(L, 1, "method"); // request["method"]="GET"

	for (const auto& pr : mp)
	{
		lua_pushstring(L, pr.second.c_str());
		lua_setfield(L, 1, pr.first.c_str()); // request[...]=...
	}

	// Parameter table
	lua_newtable(L);
	for (const auto& pr : param)
	{
		lua_pushstring(L, pr.second.c_str());
		lua_setfield(L, 2, pr.first.c_str()); // request["param"][...]=...
	}
	lua_setfield(L, 1, "param");

	lua_setglobal(L, "request");

	// Lua CGI program should fill the response table.
	// response.output will be outputed as content.
	lua_newtable(L);
	lua_setglobal(L, "response");

	logd("Preparing helper...\n");

	if (v.runCode("helper={} helper.print=function(...) local t=table.pack(...) "s +
		"if(response.output==nil) then response.output='' end " + 
		"local temp='' " +
		"for i,v in ipairs(t) do " +
		"if(#(temp)>0) then temp = temp .. '\\t' end " +
		"temp = temp .. tostring(v) " +
		"end " +
		"response.output = response.output .. temp .. '\\n' " + 
		"end ") < 0)
	{
		loge("Failed to prepare helper.\n");
		return -2;
	}

	logd("Executing lua file: %s\n", path_decoded.c_str());
	if (v.runCode(lua_code) < 0)
	{
		loge("Failed to run user lua code.\n");
		return -3;
	}

	logd("Execution finished successfully.\n");

	Response ur;
	v.getglobal("response");
	
	if (!lua_istable(L, -1)) // type(response)~="table"
	{
		logd("LuaVM: variable 'response' is not a table.\n");
		return -4;
	}

	v.pushnil();
	
	while (lua_next(L, -2))
	{
		if (lua_isstring(L,-2))  // type(key)=="string"
		{
			const char* item_name = lua_tostring(L, -2);
			const char* item_value = lua_tostring(L, -1);

			if ((!item_name) || (!item_value))
			{
				logd("LuaVM: An item cannot be converted to string. Key: %s\n", item_name);
			}
			else
			{
				if (strcmp(item_name, "output") != 0)
				{
					ur.set_raw(item_name, item_value);
				}
				else
				{
					ur.setContentRaw(item_value);
				}
			}			
		}
		lua_pop(L, 1);
	}
	ur.set_code(200);
	ur.send_with(s);

	return 0;
}

int parse_range_request(const string& range, int content_length, int& _out_beginat,int& _out_length)
{
	const char* s = range.c_str();
	const char* t = s + range.size();
	const char* p = strstr(s, "bytes=");
	if (p == NULL)
	{
		return -1;
	}
	// bytes=...
	p = p + 6;
	const char* q = strstr(p, "-");
	if (q == NULL)
	{
		return -2;
	}

	int beginat;
	if (q - p > 0)
	{
		// bytes=A-...
		sscanf(p, "%d", &beginat);
	}
	else
	{
		// bytes=-...
		beginat = -1;
	}

	q++;
	int endat;
	if (t - q > 0)
	{
		// bytes=?-B
		sscanf(q, "%d", &endat);
	}
	else
	{
		// bytes=?-
		endat = -1;
	}

	if (beginat < 0 && endat < 0)
	{
		return -3;
	}

	if (beginat < 0)
	{
		// bytes=-B
		_out_length = endat;
		_out_beginat = content_length - _out_length;
	}
	else if (endat < 0)
	{
		// bytes=A-
		_out_beginat = beginat;
		_out_length = content_length - beginat;
	}
	else
	{
		// bytes=A-B
		_out_beginat = beginat;
		_out_length = mymin(content_length - beginat, endat - beginat + 1);
	}

	return 0;
}

int request_get_handler(sock& s, const string& in_path, const string& version, const map<string, string>& mp)
{
	// URL Decode first
	string path;
	map<string, string> param;
	if (urldecode(in_path, path, param) < 0)
	{
		loge("Failed to decode url : %s\n", in_path.c_str());
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

			ans.append("<html><head><title>Index of " + path + "</title></head><body><h1>Index of " + path + "</h1><ul>");
			string real_dir = SERVER_ROOT + path;
			logd("About to list Directory: %s\n", real_dir.c_str());
			DirWalk w(real_dir);
			logd("DirWalk Created.\n");
			string filename;
			int is_dir;
			while (w.next(filename, is_dir) > 0)
			{
				string realname;
				urlencode(filename, realname);

				ans.append("<li><a href='" + realname);
				if (is_dir)
				{
					ans.append("/");
				}
				ans.append("'>" + filename);
				if (is_dir)
				{
					ans.append("/");
				}
				ans.append("</a></li>");
			}
			ans.append("</ul></body></html>");

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
		int content_length;
		if (GetFileLength(path, content_length) < 0)
		{
			/// File not readable.
			Response r;
			r.set_code(500);
			r.send_with(s);
			return 0;
		}
		else
		{
			// Requesting partial content?
			if (mp.find("Range") != mp.end())
			{
				int beginat, length;
				// FIXME: Why mp["Range"] cause compile error?
				if (parse_range_request(mp.find("Range")->second, content_length, beginat, length) < 0)
				{
					Response r;
					r.set_code(416);
					r.send_with(s);
					return 0;
				}

				logd("Range Request: begin: %d, length: %d\n", beginat, length);

				Response r;
				string content_type;
				if (GetFileContentType(path, content_type) < 0) content_type = "text/plain";
				
				if (length != content_length)
				{
					// partial content
					r.set_code(206);
					string content;
					if (GetFileContentEx(path, beginat, length, content) < 0)
					{
						/// Error while reading file.
						Response r;
						r.set_code(500);
						r.send_with(s);
						return 0;
					}
					r.setContent(content, content_type);
				}
				else
				{
					// full content
					r.set_code(200);
					string content;
					if (GetFileContent(path, content) < 0)
					{
						/// Error while reading file.
						Response r;
						r.set_code(500);
						r.send_with(s);
						return 0;
					}
					r.setContent(content, content_type);
				}

				char content_range_buff[64] = { 0 };
				sprintf(content_range_buff, "bytes %d-%d/%d", beginat, beginat + length - 1, content_length);
				r.set_raw("Content-Range", content_range_buff);

				r.set_raw("Accept-Ranges", "bytes");
				r.send_with(s);
				return 0;
			}
			else
			{
				// Just a normal request without Range in request header.
				Response r;
				r.set_code(200);
				r.set_raw("Accept-Ranges", "bytes");
				string content_type;
				if (GetFileContentType(path, content_type) < 0) content_type = "text/plain";
				if (ReadFileAndSend(r,s, path) < 0)
				{
					/// Error while reading file.
					Response r;
					r.set_code(500);
					r.send_with(s);
				}
				return 0;
			}
		}
	}
	else
	{
		// Dynamic Target
		if (request_get_dynamic_handler(s, path, version, mp, param) < 0)
		{
			Response r;
			r.set_code(500);
			r.send_with(s);
		}
		return 0;
	}
}

int request_post_dynamic_handler(sock& s, const string& path_decoded, const string& version, const map<string, string>& mp, const string& post_content)
{
	logd("Loading lua file: %s\n", path_decoded.c_str());

	string lua_code;
	int ret = GetFileContent(path_decoded, lua_code);
	if (ret < 0)
	{
		return -1;
	}

	VM v;
	auto L = v.get();
	lua_newtable(L);
	lua_pushstring(L, version.c_str());
	lua_setfield(L, 1, "http_version"); // request["http_version"]=...

	lua_pushstring(L, "POST");
	lua_setfield(L, 1, "method"); // request["method"]="POST"

	for (const auto& pr : mp)
	{
		lua_pushstring(L, pr.second.c_str());
		lua_setfield(L, 1, pr.first.c_str()); // request[...]=...
	}

	// Parameter (Posted Content). May contain binary content.
	lua_pushlstring(L, post_content.data(), post_content.size());
	lua_setfield(L, 1, "param");

	lua_setglobal(L, "request");

	// Lua CGI program should fill the response table.
	// response.output will be outputed as content.
	lua_newtable(L);
	lua_setglobal(L, "response");

	logd("Preparing helper...\n");

	if (v.runCode("helper={} helper.print=function(...) local t=table.pack(...) "s +
		"if(response.output==nil) then response.output='' end " +
		"local temp='' " +
		"for i,v in ipairs(t) do " +
		"if(#(temp)>0) then temp = temp .. '\\t' end " +
		"temp = temp .. tostring(v) " +
		"end " +
		"response.output = response.output .. temp .. '\\n' " +
		"end ") < 0)
	{
		loge("Failed to prepare helper.\n");
		return -2;
	}

	logd("Executing lua file: %s\n", path_decoded.c_str());
	if (v.runCode(lua_code) < 0)
	{
		loge("Failed to run user lua code.\n");
		return -3;
	}

	logd("Execution finished successfully.\n");

	Response ur;
	v.getglobal("response");

	if (!lua_istable(L, -1)) // type(response)~="table"
	{
		logd("LuaVM: variable 'response' is not a table.\n");
		return -4;
	}

	v.pushnil();

	while (lua_next(L, -2))
	{
		if (lua_isstring(L, -2))  // type(key)=="string"
		{
			const char* item_name = lua_tostring(L, -2);
			const char* item_value = lua_tostring(L, -1);

			if ((!item_name) || (!item_value))
			{
				logd("LuaVM: An item cannot be converted to string. Key: %s\n", item_name);
			}
			else
			{
				if (strcmp(item_name, "output") != 0)
				{
					ur.set_raw(item_name, item_value);
				}
				else
				{
					ur.setContentRaw(item_value);
				}
			}
		}
		lua_pop(L, 1);
	}
	ur.set_code(200);
	ur.send_with(s);

	return 0;
}

int request_post_handler(sock& s, const string& in_path, const string& version, const map<string, string>& mp)
{
	// URL Decode first
	string path;
	map<string, string> param;
	if (urldecode(in_path, path, param) < 0)
	{
		loge("Failed to decode url : %s\n", in_path.c_str());
		return -1;
	}

	// Request to / is invalid for POST method.
	if (endwith(path, "/"))
	{
		// invalid request.
		return -1;
	}

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
		auto iter = mp.find("Content-Length");
		int content_length;
		// Dynamic Target
		if (iter == mp.end() || sscanf(iter->second.c_str(), "%d", &content_length) != 1)
		{
			logd("Failed to get content length.\n");
			Response r;
			r.set_code(400);
			r.send_with(s);
			return 0;
		}

		// Read content
		char xtbuf[1024] = { 0 };
		string post_content;
		int done = 0;
		while (done < content_length)
		{
			int ret = s.recv(xtbuf, mymin(content_length - done, 1024));
			if (ret <= 0) return -1;
			done += ret;
			post_content.append(string(xtbuf, ret));
			memset(xtbuf, 0, 1024);
		}

		if (request_post_dynamic_handler(s, path, version, mp, post_content) < 0)
		{
			Response r;
			r.set_code(500);
			r.send_with(s);
		}
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

