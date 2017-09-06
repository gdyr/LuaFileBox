#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lfs.h"
#include "liolib.h"

#define LFS_LIBNAME "Files"

/*
** methods for file handles
*/
static const luaL_Reg flib[] = {
        {"close", io_close},
        {"flush", f_flush},
        {"lines", f_lines},
        {"read", f_read},
        {"seek", f_seek},
        {"setvbuf", f_setvbuf},
        {"write", f_write},
        {"__gc", f_gc},
        {"__tostring", f_tostring},
        {NULL, NULL}
};


static void createmeta (lua_State *L) {
        luaL_newmetatable(L, LUA_FILEHANDLE);  /* create metatable for file handles */
        lua_pushvalue(L, -1);  /* push metatable */
        lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
        luaL_setfuncs(L, flib, 0);  /* add file methods to new metatable */
        lua_pop(L, 1);  /* pop new metatable */
}

static const struct luaL_Reg fslib[] = {
        {"close", io_close},
        {"flush", io_flush},
        {"open", io_open},
        {"read", io_read},
        {"type", io_type},
        {"attributes", file_info},
        {"chdir", change_dir},
        {"currentdir", get_dir},
        {"dir", dir_iter_factory},
        {"mkdir", make_dir},
        {"rmdir", remove_dir},
        {"touch", file_utime},
        {NULL, NULL},
};

LFS_EXPORT int luaopen_lfb (lua_State *L) {
        dir_create_meta (L);
        luaL_newlib (L, fslib);
        lua_pushvalue(L, -1);
        lua_setglobal(L, LFS_LIBNAME);
        createmeta(L);

        /* create (and set) default files */
        createstdfile(L, stdin, IO_INPUT, "stdin");
        createstdfile(L, stdout, IO_OUTPUT, "stdout");
        createstdfile(L, stderr, NULL, "stderr");
        return 1;
}
