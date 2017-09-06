/*
** LuaFileSystem
** Copyright Kepler Project 2003 - 2016 (http://keplerproject.github.io/luafilesystem)
*/

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* Define 'chdir' for systems that do not implement it */
#ifdef NO_CHDIR
  #define chdir(p)	(-1)
  #define chdir_error	"Function 'chdir' not provided by system"
#else
  #define chdir_error	strerror(errno)
#endif

#ifdef _WIN32
  #define chdir(p) (_chdir(p))
  #define getcwd(d, s) (_getcwd(d, s))
  #define rmdir(p) (_rmdir(p))
  #define LFS_EXPORT __declspec (dllexport)
  #ifndef fileno
    #define fileno(f) (_fileno(f))
  #endif
#else
  #define LFS_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

LFS_EXPORT  int luaopen_lfs (lua_State *L);

#ifdef __cplusplus
}
#endif

int file_info (lua_State *L);
int change_dir (lua_State *L);
int get_dir (lua_State *L);
int make_dir (lua_State *L);
int remove_dir (lua_State *L);
int dir_iter_factory (lua_State *L);
int file_utime (lua_State *L);
int dir_create_meta (lua_State *L);