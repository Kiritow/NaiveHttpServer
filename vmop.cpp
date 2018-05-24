#include "vmop.h"
using namespace std;

VM::VM()
{
	_luavm = luaL_newstate();
	luaL_openlibs(_luavm);
	runCode("response={}");
}


VM::~VM()
{
	lua_close(_luavm);
}

int VM::runCodeEx(const function<void(lua_State*)>& prepare,
	const function<void(lua_State*)>& after,
	const string& LuaSource)
{
	bool err = luaL_loadbuffer(_luavm, LuaSource.c_str(), (int)LuaSource.size(), "LuaVM") || lua_pcall(_luavm, 0, 0, 0);
	if (err) {
		printf("%s\n", lua_tostring(_luavm, -1));
		lua_pop(_luavm, 1);
		return -1;
	}
	return 0;
}

int VM::runCode(const string& LuaSource)
{
	return runCodeEx([](lua_State*) {}, [](lua_State*) {}, LuaSource);
}