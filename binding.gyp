{
  "targets": [
    {
      "target_name": "nodelua",
      "sources": [
        "src/utils.cc",
        "src/luastate.cc",
	      "src/nodelua.cc",
        "src/yajlbind.cc",
        
        "src/compat-5.2.c",
        
        "lua5.1/lapi.c",
        "lua5.1/lcode.c",
        "lua5.1/ldebug.c",
        "lua5.1/ldo.c",
        "lua5.1/ldump.c",
        "lua5.1/lfunc.c",
        "lua5.1/lgc.c",
        "lua5.1/llex.c",
        "lua5.1/lmem.c",
        "lua5.1/lobject.c",
        "lua5.1/lopcodes.c",
        "lua5.1/lparser.c",
        "lua5.1/lstate.c",
        "lua5.1/lstring.c",
        "lua5.1/ltable.c",
        "lua5.1/ltm.c",
        "lua5.1/lundump.c",
        "lua5.1/lvm.c",
        "lua5.1/lzio.c",
        "lua5.1/lauxlib.c",
        "lua5.1/lbaselib.c",
        "lua5.1/ldblib.c",
        "lua5.1/liolib.c",
        "lua5.1/lmathlib.c",
        "lua5.1/loslib.c",
        "lua5.1/ltablib.c",
        "lua5.1/lstrlib.c",
        "lua5.1/loadlib.c",
        "lua5.1/linit.c",

        "yajl-2.1.0/src/yajl.c", 
        "yajl-2.1.0/src/yajl_alloc.c", 
        "yajl-2.1.0/src/yajl_buf.c", 
        "yajl-2.1.0/src/yajl_encode.c", 
        "yajl-2.1.0/src/yajl_gen.c", 
        "yajl-2.1.0/src/yajl_lex.c", 
        "yajl-2.1.0/src/yajl_parser.c",
        "yajl-2.1.0/src/yajl_tree.c"
        
	],
    "defines" : ["LUA_ANSI", "LUA_USE_DLOPEN"],

      "include_dirs": [
        "lua-5.2.4/src",
        "yajl-2.1.0/src/api",
        "<!(node -e \"require('nan')\")"
        ],
      "libraries": [
        "-ldl",
        "<!(echo $NODELUA_FLAGS)"
      ]
    }
  ]
}
