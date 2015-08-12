#define BUILDING_NODELUA
#include "luastate.h"
#include <string.h>
#include <vector>

#if LUA_VERSION_NUM >= 502
static lua_State* lua_open() {
  return luaL_newstate();
}
#endif

int luaopen_cjson(lua_State* L);
int luaopen_cjson_safe(lua_State* L);

/// @todo : move to state
typedef std::map<std::string, 
  Nan::Callback* > functions_map_t;
functions_map_t functions;

class lua_lock {
private:
  LuaState* state_;
public:
   explicit lua_lock( LuaState* state ) : state_(state) {
      uv_mutex_lock(&state_->mutex_);
   }
   ~lua_lock() {
      uv_mutex_unlock(&state_->mutex_);
   }
};

class async_baton{
public:
  virtual ~async_baton() {}
  Nan::Callback callback;
  bool error;
  char msg[1000];
  LuaState* state;
};

class async_data_baton : public async_baton {
public:
    async_data_baton() : result(LUA_NOREF) {}
    std::string data;
    int result;
};

class async_call_baton : public async_data_baton {
public:
    std::vector<int> args;
};



struct simple_baton{
  Nan::Callback callback;
  int data;
  int result;
  LuaState* state;
};


void do_file(uv_work_t *req){
  async_data_baton* baton = static_cast<async_data_baton*>(req->data);
  lua_lock lock(baton->state);
  int top = lua_gettop(baton->state->lua_);
  if(luaL_dofile(baton->state->lua_, baton->data.c_str())){
    baton->error = true;
    sprintf(baton->msg, "Exception In File %s Has Failed:\n%s\n", baton->data.c_str(), lua_tostring(baton->state->lua_, -1));
  } else {
    if(lua_gettop(baton->state->lua_)>top){
      baton->result = luaL_ref(baton->state->lua_,LUA_REGISTRYINDEX);
    }
  }
  lua_pop(baton->state->lua_,lua_gettop(baton->state->lua_)-top);
}


void do_gc(uv_work_t *req){
  simple_baton* baton = static_cast<simple_baton*>(req->data);
  lua_lock lock(baton->state);
  baton->result = lua_gc(baton->state->lua_, baton->data, 0);
}


void do_status(uv_work_t *req){
  simple_baton* baton = static_cast<simple_baton*>(req->data);
  lua_lock lock(baton->state);
  baton->result = lua_status(baton->state->lua_);
}


void simple_after(uv_work_t *req, int status){
  
  Nan::HandleScope scope;

  simple_baton* baton = static_cast<simple_baton*>(req->data);

  const int argc = 1;
  v8::Local<v8::Value> argv[] = { Nan::New<v8::Number>(baton->result) };

  Nan::TryCatch try_catch;

  if(!baton->callback.IsEmpty()){
    baton->callback.Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }

  delete baton;
  delete req;

  if(try_catch.HasCaught()){
    Nan::FatalException(try_catch);
  }
}

void do_string(uv_work_t *req){
  async_data_baton* baton = static_cast<async_data_baton*>(req->data);
  lua_lock lock(baton->state);
  int top = lua_gettop(baton->state->lua_);
  if(luaL_dostring(baton->state->lua_, baton->data.c_str())){
    baton->error = true;
    sprintf(baton->msg, "Exception Of Lua Code Has Failed:\n%s\n", lua_tostring(baton->state->lua_, -1));
  } else {
    if(lua_gettop(baton->state->lua_)>top){
      baton->result = luaL_ref(baton->state->lua_,LUA_REGISTRYINDEX);
    } 
  }
  lua_pop(baton->state->lua_,lua_gettop(baton->state->lua_)-top);
}

void do_call_global(uv_work_t *req) {
  async_call_baton* baton = static_cast<async_call_baton*>(req->data);
  lua_lock lock(baton->state);
  lua_getglobal(baton->state->lua_,baton->data.c_str());
  if (!lua_isfunction(baton->state->lua_,-1)) {
    baton->error = true;
    sprintf(baton->msg, "Not found global function %s\n", baton->data.c_str());
    lua_pop(baton->state->lua_,1);
    return;
  }
  for (std::vector<int>::const_iterator i=baton->args.begin();i!=baton->args.end();++i) {
    lua_rawgeti(baton->state->lua_,LUA_REGISTRYINDEX,*i);
    luaL_unref(baton->state->lua_,LUA_REGISTRYINDEX,*i);
  }
 
  if (lua_pcall(baton->state->lua_,baton->args.size(),1,0)!=LUA_OK) {
    baton->error = true;
    sprintf(baton->msg, "Exception Of Lua Code Has Failed:\n%s\n", lua_tostring(baton->state->lua_, -1));
    lua_pop(baton->state->lua_,1);
    return;
  } else {
    baton->result = luaL_ref(baton->state->lua_,LUA_REGISTRYINDEX);
  }
}

void async_after(uv_work_t *req, int status){
  
  Nan::HandleScope scope;

  async_data_baton* baton = (async_data_baton *)req->data;
  

  v8::Local<v8::Value> argv[2];
  const int argc = 2;

  if(baton->error){
    argv[0] = Nan::New(baton->msg).ToLocalChecked();
    argv[1] = Nan::Undefined();
  } else {
    argv[0] = Nan::Undefined();
    if(baton->result != LUA_NOREF){
      lua_lock lock(baton->state);
      lua_rawgeti(baton->state->lua_,LUA_REGISTRYINDEX,baton->result);
      luaL_unref(baton->state->lua_,LUA_REGISTRYINDEX,baton->result);
      argv[1] = lua_to_value(baton->state->lua_, -1);
      lua_pop(baton->state->lua_,1);
    } else{
      argv[1] = Nan::Undefined();
    }
  }

  Nan::TryCatch try_catch;

  if(!baton->callback.IsEmpty()){
    baton->callback.Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }

  delete baton;
  delete req;

  if(try_catch.HasCaught()){
    Nan::FatalException(try_catch);
  }
}


LuaState::LuaState(){
  uv_mutex_init(&mutex_);
}
LuaState::~LuaState(){
  uv_mutex_destroy(&mutex_);
}


void LuaState::Init(v8::Handle<v8::Object> target){
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("LuaState").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(2);

  tpl->PrototypeTemplate()->Set(Nan::New("doFileSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(DoFileSync)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("doFile").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(DoFile)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("doStringSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(DoStringSync)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("doString").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(DoString)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("setGlobal").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(SetGlobal)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("getGlobal").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(GetGlobal)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("callGlobalSync").ToLocalChecked(),
        Nan::New<v8::FunctionTemplate>(CallGlobalSync)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("callGlobal").ToLocalChecked(),
        Nan::New<v8::FunctionTemplate>(CallGlobal)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("status").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(Status)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("statusSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(StatusSync)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("collectGarbage").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(CollectGarbage)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("collectGarbageSync").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(CollectGarbageSync)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("close").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(Close)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("getName").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(GetName)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("registerFunction").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(RegisterFunction)->GetFunction());

  tpl->PrototypeTemplate()->Set(Nan::New("push").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(Push)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("pop").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(Pop)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("getTop").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(GetTop)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("setTop").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(SetTop)->GetFunction());
  tpl->PrototypeTemplate()->Set(Nan::New("replace").ToLocalChecked(),
				Nan::New<v8::FunctionTemplate>(Replace)->GetFunction());

  //Nan::Persistent<v8::Function> constructor = ;

  target->Set(Nan::New("LuaState").ToLocalChecked(), tpl->GetFunction());
}


int LuaState::CallFunction(lua_State* L){
  int n = lua_gettop(L);

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
  lua_lock lock(obj);
  if(luaL_dofile(obj->lua_, file_name.c_str())){
    char buf[1000];
    sprintf(buf, "Exception Of File %s Has Failed:\n%s\n", file_name.c_str(), lua_tostring(obj->lua_, -1));
    Nan::ThrowError(buf);
    return;
  }

  if(lua_gettop(obj->lua_)){
    info.GetReturnValue().Set(lua_to_value(obj->lua_, -1));
  } else{
    return;
  }
}


NAN_METHOD(LuaState::DoFile){
  Nan::HandleScope scope;

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.doFile Requires At Least 1 Argument");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.doFile First Argument Must Be A String");
    return;
  }

  if(info.Length() > 1 && !info[1]->IsFunction()){
    Nan::ThrowError("LuaState.doFile Second Argument Must Be A Function");
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  async_data_baton* baton = new async_data_baton();
  baton->data = get_str(info[0]);
  baton->state = obj;
  obj->Ref();

  if(info.Length() > 1){
    baton->callback.SetFunction(v8::Local<v8::Function>::Cast(info[1]));
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_file, async_after);

}


NAN_METHOD(LuaState::DoStringSync) {
  Nan::HandleScope scope;

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.doStringSync Requires 1 Argument");
    return;
  }

  std::string lua_code = get_str(info[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);

  if(luaL_dostring(obj->lua_, lua_code.c_str())){
    char buf[1000];
    sprintf(buf, "Execution Of Lua Code Has Failed:\n%s\n", lua_tostring(obj->lua_, -1));
    Nan::ThrowError(buf);
    return;
  }

  if(lua_gettop(obj->lua_)){
    info.GetReturnValue().Set(lua_to_value(obj->lua_, -1));
  }
}


NAN_METHOD(LuaState::DoString){
  Nan::HandleScope scope;

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.doString Requires At Least 1 Argument");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.doString: First Argument Must Be A String");
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);

  async_data_baton* baton = new async_data_baton();
  baton->data = get_str(info[0]);
  baton->state = obj;
  obj->Ref();

  if(info.Length() > 1 && !info[1]->IsFunction()){
    Nan::ThrowError("LuaState.doString Second Argument Must Be A Function");
    return;
  }

  if(info.Length() > 1){
    baton->callback.SetFunction(v8::Local<v8::Function>::Cast(info[1]));
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_string, async_after);

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
  lua_lock lock(obj);

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
  lua_lock lock(obj);

  lua_getglobal(obj->lua_, global_name.c_str());

  info.GetReturnValue().Set(lua_to_value(obj->lua_, -1));
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
  lua_lock lock(obj);

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


NAN_METHOD(LuaState::CallGlobal) {
 
  if(info.Length() < 1){
    Nan::ThrowError("LuaState.callGlobal Requires 1 Argument");
    return;
  }

  if(!info[0]->IsString()){
    Nan::ThrowError("LuaState.callGlobal Argument 1 Must Be A String");
    return;
  }


  int cb_index = 0;
  int args_index = 0;

  if(info.Length()>1){
    if (info[1]->IsFunction()) {
      cb_index = 1;
    } else if (info[1]->IsArray()) {
      args_index = 1;
    } else {
      Nan::ThrowError("LuaState.callGlobal Argument 2 Must Be A Function or Array");
      return;
    }
  }

  if(info.Length()>2){
    if (info[1]->IsArray()&&info[2]->IsFunction()) {
      args_index = 1;
      cb_index = 2;
    } else {
      Nan::ThrowError("LuaState.callGlobal Argument 2 Must Be A Array and Agument 3 Must Be Function");
      return;
    }
  }


  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());

  
  async_call_baton* baton = new async_call_baton();
  baton->data = get_str(info[0]);
  baton->state = obj;
  obj->Ref();


  if(cb_index){
    baton->callback.SetFunction(v8::Local<v8::Function>::Cast(info[cb_index]));
  }

  if (args_index) {
    lua_lock lock(obj);
    v8::Local<v8::Array> a = v8::Local<v8::Array>::Cast(info[args_index]);
    for (uint32_t i=0;i<a->Length();++i) {
      push_value_to_lua(obj->lua_,a->Get(i));
      baton->args.push_back(luaL_ref(obj->lua_,LUA_REGISTRYINDEX));
    }
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_call_global, async_after);

}

NAN_METHOD(LuaState::Close){
  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_close(obj->lua_);
}


NAN_METHOD(LuaState::Status){
 
  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  simple_baton* baton = new simple_baton();
  baton->state = obj;
  obj->Ref();

  if(info.Length() > 0 && !info[0]->IsFunction()){
    Nan::ThrowError("LuaState.status First Argument Must Be A Function");
    return;
  }

  if(info.Length() > 0){
    baton->callback.SetFunction(v8::Local<v8::Function>::Cast(info[0]));
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_status, simple_after);
}


NAN_METHOD(LuaState::StatusSync){
  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);
  int status = lua_status(obj->lua_);

  info.GetReturnValue().Set(Nan::New<v8::Number>(status));
}


NAN_METHOD(LuaState::CollectGarbage){
 
  if(info.Length() < 1){
    Nan::ThrowError("LuaState.collectGarbage Requires 1 Argument");
    return;
  }

  if(!info[0]->IsNumber()){
    Nan::ThrowError("LuaSatte.collectGarbage Argument 1 Must Be A Number, try nodelua.GC.[TYPE]");
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  int type = (int)info[0]->ToNumber()->Value();

  simple_baton* baton = new simple_baton();
  baton->data = type;
  baton->state = obj;
  obj->Ref();

  if(info.Length() > 1 && !info[1]->IsFunction()){
    Nan::ThrowError("LuaState.collectGarbage Second Argument Must Be A Function");
    return;
  }

  if(info.Length() > 1){
    baton->callback.SetFunction(v8::Local<v8::Function>::Cast(info[1]));
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_gc, simple_after);

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
  lua_lock lock(obj);

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
  lua_lock lock(obj);

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


NAN_METHOD(LuaState::Push) {

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.push Requires 1 Argument");
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);

  push_value_to_lua(obj->lua_, info[0]);

}


NAN_METHOD(LuaState::Pop) {
 
  int pop_n = 1;
  if(info.Length() > 0 && info[0]->IsNumber()){
    pop_n = (int)info[0]->ToNumber()->Value();
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);
  lua_pop(obj->lua_, pop_n);

}


NAN_METHOD(LuaState::GetTop) {
 
  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);
  int n = lua_gettop(obj->lua_);

  info.GetReturnValue().Set(Nan::New(n));
}


NAN_METHOD(LuaState::SetTop) {
  
  int set_n = 0;
  if(info.Length() > 0 && info[0]->IsNumber()){
    set_n = (int)info[0]->ToNumber()->Value();
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);
  lua_settop(obj->lua_, set_n);

}


NAN_METHOD(LuaState::Replace) {
  Nan::HandleScope scope;

  if(info.Length() < 1){
    Nan::ThrowError("LuaState.replace Requires 1 Argument");
    return;
  }

  if(!info[0]->IsNumber()){
    Nan::ThrowError("LuaState.replace Argument 1 Must Be A Number");
    return;
  }

  int index = (int)info[0]->ToNumber()->Value();

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(info.This());
  lua_lock lock(obj);
  lua_replace(obj->lua_, index);

}
