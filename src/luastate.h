#ifndef LUASTATE_H
#define LUASTATE_H

#include <map>
#include <string>
#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>
#include <nan.h>

//using namespace v8;

#include "utils.h"

extern "C"{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class LuaState : public node::ObjectWrap{
 public:
  lua_State* lua_;
  std::string name_;
  uv_mutex_t  mutex_;

  static void Init(v8::Handle<v8::Object> target);
  static int CallFunction(lua_State* L);

 private:
  LuaState();
  ~LuaState();

  static NAN_METHOD(New);
  static NAN_METHOD(Close);
  static NAN_METHOD(GetName);

  static NAN_METHOD(CollectGarbage);
  static NAN_METHOD(CollectGarbageSync);

  static NAN_METHOD(Status);
  static NAN_METHOD(StatusSync);


  static NAN_METHOD(DoFileSync);
  static NAN_METHOD(DoFile);

  static NAN_METHOD(DoStringSync);
  static NAN_METHOD(DoString);

  static NAN_METHOD(SetGlobal);
  static NAN_METHOD(GetGlobal);

  static NAN_METHOD(CallGlobal);
  static NAN_METHOD(CallGlobalSync);

  static NAN_METHOD(RegisterFunction);

  static NAN_METHOD(Push);
  static NAN_METHOD(Pop);
  static NAN_METHOD(GetTop);
  static NAN_METHOD(SetTop);
  static NAN_METHOD(Replace);
};
#endif
