#include <stdlib.h>
#include <assert.h>
#include "utils.h"
using namespace v8;

lua_stack_check::lua_stack_check(lua_State* L,int delta) : m_L(L),m_top(0) {
  m_top = lua_gettop(L) + delta;
}
lua_stack_check::~lua_stack_check() {
  int now = lua_gettop(m_L);
  assert(now == m_top);
}

std::string get_str(v8::Local<v8::Value> val){
  v8::Isolate* isolate;
  isolate = v8::Isolate::GetCurrent();
  if(!val->IsString()){
    isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Argument Must Be A String")));
    return NULL;
  }

  v8::String::Utf8Value val_string(val);
  return std::string(*val_string,val_string.length());
}


v8::Local<v8::Value> lua_to_value(lua_State* L, int i){
  v8::Isolate* isolate;
  isolate = v8::Isolate::GetCurrent();
  switch(lua_type(L, i)){
  case LUA_TBOOLEAN:
    return v8::Local<v8::Boolean>::New(isolate, v8::Boolean::New(isolate, (int)lua_toboolean(L, i)));
    break;
  case LUA_TNUMBER:
    return v8::Local<v8::Number>::New(isolate, v8::Number::New(isolate, lua_tonumber(L, i)));
    break;
  case LUA_TSTRING:
    return v8::String::NewFromUtf8(isolate, (char *)lua_tostring(L, i));
    break;
  case LUA_TTABLE:
    {
      v8::Local<v8::Object> obj = v8::Object::New(isolate);
      int idx = i < 0 ? (i-1) : i;
      lua_pushnil(L);
      while(lua_next(L, idx) != 0){
	     v8::Local<v8::Value> key = lua_to_value(L, -2);
	     v8::Local<v8::Value> value = lua_to_value(L, -1);
	     obj->Set(key, value);
	     lua_pop(L, 1);
      }
      return obj;
      break;
    }
  default:
    return v8::Local<v8::Primitive>::New(isolate, v8::Undefined(isolate));
    break;
  }
}

void push_value_to_lua(lua_State* L, v8::Handle<v8::Value> value){
  v8::Isolate* isolate;
  isolate = v8::Isolate::GetCurrent();
  if (value.IsEmpty()) {
    lua_pushnil(L);
    return;
  }
  if(value->IsString()){
    lua_pushstring(L, get_str(v8::Local<v8::Value>::New(isolate, value)).c_str());
  }else if(value->IsNumber()){
    lua_Number n_value = value->ToNumber()->Value();
    lua_pushnumber(L, n_value);
  }else if(value->IsBoolean()){
    int b_value = (int)value->ToBoolean()->Value();
    lua_pushboolean(L, b_value);
  }else if(value->IsArray()){
    lua_newtable(L);
    v8::Local<v8::Value> lvalue(value);
    v8::Local<v8::Array> values = v8::Local<v8::Array>::Cast(lvalue);
    for(uint32_t i = 0; i < values->Length(); ++i){
      v8::Local<v8::Value> val = values->Get(i);
      lua_pushinteger(L,i+1);
      push_value_to_lua(L, val);
      lua_settable(L, -3);
    }
  }else if(value->IsObject()){
    lua_newtable(L);
    v8::Local<v8::Object> obj = value->ToObject();
    v8::Local<v8::Array> keys = obj->GetPropertyNames();
    for(uint32_t i = 0; i < keys->Length(); ++i){
      v8::Local<v8::Value> key = keys->Get(i);
      v8::Local<v8::Value> val = obj->Get(key);
      push_value_to_lua(L, key);
      push_value_to_lua(L, val);
      lua_settable(L, -3);
    }
  }else{
    lua_pushnil(L);
  }
}
