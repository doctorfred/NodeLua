#define BUILDING_NODELUA
#include "luastate.h"
#include <string.h>
#include <vector>
extern "C"{
#include "compat-5.2.h"
}

int luaopen_cjson(lua_State* L);
int luaopen_cjson_safe(lua_State* L);

/// @todo : move to state
typedef std::map<std::string, 
  Nan::Callback* > functions_map_t;
functions_map_t functions;


LuaState::LuaState(){
}
LuaState::~LuaState(){
}


void LuaState::Init(v8::Handle<v8::Object> target){
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("LuaState").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(2);

  tpl->PrototypeTemplate()->Set(Nan::New("doFileSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(DoFileSync)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("doStringSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(DoStringSync)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("setGlobal").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(SetGlobal)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("getGlobal").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(GetGlobal)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("callGlobalSync").ToLocalChecked(),
        Nan::New<v8::FunctionTemplate>(CallGlobalSync)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("collectGarbageSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(CollectGarbageSync)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("close").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(Close)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("getName").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(GetName)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("registerFunction").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(RegisterFunction)->GetFunction());

  target->Set(Nan::New("LuaState").ToLocalChecked(), tpl->GetFunction());
}


int LuaState::CallFunction(lua_State* L){
  int n = lua_gettop(L);
  lua_stack_check check(L,1);

  const char * func_name = lua_tostring(L, lua_upvalueindex(1));

  const unsigned argc = n;
  v8::Local<v8::Value>* argv = new v8::Local<v8::Value>[argc];
  int i;
  for(i = 1; i <= n; ++i){
    argv[i - 1] = lua_to_value(L, i);
  }

  v8::Handle<v8::Value> ret_val = Nan::Undefined();

  functions_map_t::iterator iter;
  for(iter = functions.begin(); iter != functions.end(); iter++){
    if(strcmp(iter->first.c_str(), func_name) == 0){
      Nan::Callback* func = iter->second;
      ret_val = func->Call(Nan::GetCurrentContext()->Global(), argc, argv);
      break;
    }
  }

  delete [] argv;

  push_value_to_lua(L, ret_val);
  return 1;
}


NAN_METHOD(LuaState::New){
  Nan::HandleScope scope;

  if(!info.IsConstructCall()) {
    return Nan::ThrowError("LuaState Requires The 'new' Operator To Create An Instance");
  }

  if(info.Length() < 1){
    return Nan::ThrowError("LuaState Requires 1 Argument");
  }

  if(!info[0]->IsString()){
    return Nan::ThrowError("LuaState First Argument Must Be A String");
  }

  LuaState* obj = new LuaState();
  obj->name_ = get_str(info[0]);
  obj->lua_ = lua_open();
  luaL_openlibs(obj->lua_);
    static const luaL_Reg loadedlibs[] = {
        {"cjson", luaopen_cjson},
        {"cjson.safe", luaopen_cjson_safe},
        {NULL, NULL}
    };

    const luaL_Reg *lib;
    /* call open functions from 'loadedlibs' and set results to global table */
    for (lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(obj->lua_, lib->name, lib->func, 1);
        lua_pop(obj->lua_, 1);  /* remove lib */
    }
    
  obj->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}


NAN_METHOD(LuaState::GetName){
  Nan::HandleScope scope;

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  info.GetReturnValue().Set(Nan::New(obj->name_.c_str()).ToLocalChecked());
}


NAN_METHOD(LuaState::DoFileSync){
  Nan::HandleScope scope;

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.doFileSync Takes Only 1 Argument");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.doFileSync Argument 1 Must Be A String");
    return;
  }

  std::string file_name = get_str(info[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  int top = lua_gettop(obj->lua_);
  lua_stack_check check(obj->lua_,0);

  if(luaL_dofile(obj->lua_, file_name.c_str())){
    char buf[1000];
    snprintf(buf, 1000, "Exception Of File %s Has Failed:\n%s\n", file_name.c_str(), lua_tostring(obj->lua_, -1));
    lua_pop(obj->lua_,1);
    Nan::ThrowError(buf);
    return;
  }
  top = lua_gettop(obj->lua_) - top;

  if(top > 0){
    info.GetReturnValue().Set(lua_to_value(obj->lua_, -1));
    lua_pop(obj->lua_,top);
  }

}


NAN_METHOD(LuaState::DoStringSync) {
  Nan::HandleScope scope;

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.doStringSync Requires 1 Argument");
    return;
  }

  std::string lua_code = get_str(info[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  int top = lua_gettop(obj->lua_);
  lua_stack_check check(obj->lua_,0);

  if(luaL_dostring(obj->lua_, lua_code.c_str())){
    char buf[1000];
    snprintf(buf, 1000, "Execution Of Lua Code Has Failed:\n%s\n", lua_tostring(obj->lua_, -1));
    lua_pop(obj->lua_,1);
    Nan::ThrowError(buf);
    return;
  }

  top = lua_gettop(obj->lua_) - top;

  if(top > 0){
    info.GetReturnValue().Set(lua_to_value(obj->lua_, -1));
    lua_pop(obj->lua_,top);
  }
}



NAN_METHOD(LuaState::SetGlobal) {
  
  if(info.Length() < 2){
    Nan::ThrowError("LuaState.setGlobal Requires 2 Arguments");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.setGlobal Argument 1 Must Be A String");
    return;
  }

  std::string global_name = get_str(info[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_stack_check check(obj->lua_,0);
  push_value_to_lua(obj->lua_, info[1]);
  lua_setglobal(obj->lua_, global_name.c_str());
}

NAN_METHOD(LuaState::GetGlobal) {
  if(info.Length() < 1){
    Nan::ThrowError("LuaState.getGlobal Requires 1 Argument");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.getGlobal Argument 1 Must Be A String");
    return;
  }

  std::string global_name = get_str(info[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_stack_check check(obj->lua_,0);

  lua_getglobal(obj->lua_, global_name.c_str());

  info.GetReturnValue().Set(lua_to_value(obj->lua_, -1));
  lua_pop(obj->lua_,1);

}

NAN_METHOD(LuaState::CallGlobalSync) {
 
  if(info.Length() < 1){
    Nan::ThrowError("LuaState.callGlobalSync Requires 1 Argument");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.callGlobalSync Argument 1 Must Be A String");
    return;
  }


  std::string global_name = get_str(info[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_stack_check check(obj->lua_,0);

  lua_getglobal(obj->lua_, global_name.c_str());
  if (!lua_isfunction(obj->lua_,-1)) {
    lua_pop(obj->lua_,1);
    Nan::ThrowError("LuaState.callGlobalSync not found requiret global function");
    return;
  }
  int lua_args = 0;
  if (info.Length()>1) {
    if (info[1]->IsArray()) {
       v8::Local<v8::Array> largs = v8::Local<v8::Array>::Cast(info[1]);
       for (uint32_t i=0;i<largs->Length();++i) {
          v8::Local<v8::Value> arg = largs->Get(i);
          push_value_to_lua(obj->lua_,arg);
       }
       lua_args = largs->Length();
    } else {
      lua_pop(obj->lua_,1);
      Nan::ThrowError("LuaState.callGlobalSync Argument 2 Must Be Array");
      return;
    }
  }

  int res = lua_pcall(obj->lua_,lua_args,1,0);

  if(res!=LUA_OK){
    char buf[1000];
    snprintf(buf,1000, "Execution Of Lua Code Has Failed:\n%s\n", lua_tostring(obj->lua_, -1));
    lua_pop(obj->lua_,1);
    Nan::ThrowError(buf);
    return;
  }

  v8::Local<v8::Value> ret_val = lua_to_value(obj->lua_, -1);
  lua_pop(obj->lua_,1);
  info.GetReturnValue().Set(ret_val);

}

NAN_METHOD(LuaState::Close){
  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_close(obj->lua_);
}




NAN_METHOD(LuaState::CollectGarbageSync){

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.collectGarbageSync Requires 1 Argument");
    return;
  }

  if(!info[0]->IsNumber()){
    Nan::ThrowError("LuaSatte.collectGarbageSync Argument 1 Must Be A Number, try nodelua.GC.[TYPE]");
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());

  int type = (int)info[0]->ToNumber()->Value();
  int gc = lua_gc(obj->lua_, type, 0);

  info.GetReturnValue().Set(Nan::New(gc));
}


NAN_METHOD(LuaState::RegisterFunction){
  
  if(info.Length() < 1){
    Nan::ThrowError("nodelua.registerFunction Must Have 2 Arguments");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("nodelua.registerFunction Argument 1 Must Be A String");
    return;
  }

  if(!info[1]->IsFunction()){
    Nan::ThrowError("nodelua.registerFunction Argument 2 Must Be A Function");
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_stack_check check(obj->lua_,0);

  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(info[1]);
  std::string func_name = get_str(info[0]);
  v8::Local<v8::String> func_key = v8::String::Concat(Nan::New(func_name.c_str()).ToLocalChecked(),
    Nan::New(":").ToLocalChecked());
  func_key = v8::String::Concat(func_key, Nan::New(obj->name_.c_str()).ToLocalChecked());
  functions.insert(std::make_pair(get_str(func_key), new Nan::Callback(func) ));

  lua_pushstring(obj->lua_, get_str(func_key).c_str());
  lua_pushcclosure(obj->lua_, CallFunction, 1);
  lua_setglobal(obj->lua_, func_name.c_str());

}

