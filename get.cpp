#include "get.h"
#include "vmop.h"
#include "request.h"
#include "response.h"
#include "util.h"
#include "config.h"
#include "log.h"
#include "dirop.h"
#include "parser.h"
#include <cstring>
using namespace std;

int request_get_dynamic_handler(sock& s, const string& path_decoded, const string& version, const map<string, string>& mp, const map<string, string>& param)
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

int request_get_handlerK(conn_data& conn,const Request& req)
{
	// URL Decode first
	string path;
	map<string, string> url_param;
	if (urldecode(req.path, path, url_param) < 0)
	{
		loge("Failed to decode url : %s\n", req.path.c_str());
		return -1;
	}

	// Request to / could be dispatched to /index.html,/index.lua
	if (endwith(path, "/"))
	{
		Request xreq = req;
		xreq.path += "index.html";
		if (request_get_handlerK(s, path + "index.html", version, mp) < 0 &&
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
				if (ReadFileAndSend(r, s, path) < 0)
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