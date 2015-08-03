{
  "targets": [
    {
      "target_name": "nodelua",
      "sources": [
        "src/utils.cc",
        "src/luastate.cc",
	      "src/nodelua.cc",

        "lua-5.2.4/src/lapi.c",
        "lua-5.2.4/src/lauxlib.c", 
        "lua-5.2.4/src/lbaselib.c", 
        "lua-5.2.4/src/lbitlib.c", 
        "lua-5.2.4/src/lcode.c", 
        "lua-5.2.4/src/lcorolib.c", 
        "lua-5.2.4/src/lctype.c",
        "lua-5.2.4/src/ldblib.c", 
        "lua-5.2.4/src/ldebug.c", 
        "lua-5.2.4/src/ldo.c", 
        "lua-5.2.4/src/ldump.c", 
        "lua-5.2.4/src/lfunc.c", 
        "lua-5.2.4/src/lgc.c", 
        "lua-5.2.4/src/llex.c", 
        "lua-5.2.4/src/lmathlib.c",
        "lua-5.2.4/src/lmem.c", 
        "lua-5.2.4/src/loadlib.c", 
        "lua-5.2.4/src/lobject.c", 
        "lua-5.2.4/src/lopcodes.c", 
        "lua-5.2.4/src/lparser.c", 
        "lua-5.2.4/src/lstate.c", 
        "lua-5.2.4/src/lstring.c",
        "lua-5.2.4/src/lstrlib.c", 
        "lua-5.2.4/src/ltable.c", 
        "lua-5.2.4/src/ltablib.c", 
        "lua-5.2.4/src/ltm.c", 
        "lua-5.2.4/src/lundump.c", 
        "lua-5.2.4/src/lvm.c", 
        "lua-5.2.4/src/lzio.c",
        "lua-5.2.4/src/linit.c",
        "lua-5.2.4/src/liolib.c",
        "lua-5.2.4/src/loslib.c"
	],
      "include_dirs": [
        "lua-5.2.4/src",
        "<!(node -e \"require('nan')\")"
        ],
      "defines" : ["LUA_ANSI"],
      "libraries": [
        "-ldl"
	]
    }
  ]
}
