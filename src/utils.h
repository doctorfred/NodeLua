#ifndef LUAUTILS_H
#define LUAUTILS_H

#include <string>
#include <node.h>

extern "C"{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

std::string get_str(v8::Local<v8::Value> val);
v8::Local<v8::Value> lua_to_value(lua_State* L, int);
void push_value_to_lua(lua_State* L, v8::Handle<v8::Value> value);

class lua_stack_check {
private:
	lua_State* m_L;
	int m_top;
public:
	explicit lua_stack_check(lua_State* L,int delta);
	~lua_stack_check();
};

#endif
