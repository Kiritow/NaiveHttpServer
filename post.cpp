#include "post.h"
#include "vmop.h"
#include "request.h"
#include "response.h"
#include "util.h"
#include "config.h"
#include "log.h"
#include "dirop.h"
#include <cstring>
using namespace std;

static int request_handler_post_dynamic(const Request& req, Response& res,
	const string& path_decoded, const map<string, string>& url_param) 
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
	lua_pushstring(L, req.http_version.c_str());
	lua_setfield(L, 1, "http_version"); // request["http_version"]=...

	lua_pushstring(L, "POST");
	lua_setfield(L, 1, "method"); // request["method"]="POST"

	for (const auto& pr : req.header)
	{
		lua_pushstring(L, pr.second.c_str());
		lua_setfield(L, 1, pr.first.c_str()); // request[...]=...
	}

	// Parameter (Posted Content). May contain binary content.
	lua_pushlstring(L, req.data.data(), req.data.size());
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
					res.set_raw(item_name, item_value);
				}
				else
				{
					res.setContentRaw(item_value);
				}
			}
		}
		lua_pop(L, 1);
	}

	res.set_code(200);
	return 0;
}

int request_handler_post(const Request& req, Response& res)
{
	// URL decoded path
	string path;
	map<string, string> url_param;
	if (urldecode(req.path, path, url_param) < 0)
	{
		loge("Failed to decode url : %s\n", req.path.c_str());
		return -1;
	}

	// Request to / would be dispatched to /index.lua
	if (endwith(path, "/"))
	{
		Request xreq = req;
		xreq.path += "index.lua";
		if (request_handler_post(xreq, res) < 0)
		{
			res.set_code(404);
		}
		return 0;
	}

	int request_type = get_request_path_type(path);
	if (request_type < 0)
	{
		res.set_code(404);
		return 0;
	}
	else if (request_type == 0)
	{
		// Static Target
		// POST on static target is not allowed.
		res.set_code(405);
		return 0;
	}
	else
	{
		if (request_handler_post_dynamic(req,res,path,url_param) < 0)
		{
			res.set_code(500);
		}
		return 0;
	}
}