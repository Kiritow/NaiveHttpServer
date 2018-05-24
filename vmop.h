#pragma once
#include <string>
#include <functional>
#include "LuaSrc/include/lua.hpp"

class VM
{
public:
	VM();
	/// NonMoveable,NonCopyable
	VM(const VM&) = delete;
	VM& operator = (const VM&) = delete;
	VM(VM&&) = delete;
	VM& operator = (VM&&) = delete;
	~VM();

	int runCodeEx(const std::function<void(lua_State*)>& prepare, 
		const std::function<void(lua_State*)>& after,
		const std::string& LuaSource);

	int runCode(const std::string& LuaSource);

	// ONLY For Experimental Use.
	// This method should be removed in future.
	lua_State* get();
private:
	lua_State * _luavm;
};
