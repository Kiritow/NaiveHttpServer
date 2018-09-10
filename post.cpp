#include "post.h"
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