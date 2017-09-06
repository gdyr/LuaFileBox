#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define IO_PREFIX       "_IO_"
#define IOPREF_LEN      (sizeof(IO_PREFIX)/sizeof(char) - 1)
#define IO_INPUT        (IO_PREFIX "input")
#define IO_OUTPUT       (IO_PREFIX "output")

int io_close (lua_State *L);
int io_flush (lua_State *L);
int f_flush (lua_State *L);
int f_seek (lua_State *L);
int f_write (lua_State *L);
int f_lines (lua_State *L);
int f_read (lua_State *L);
int f_setvbuf (lua_State *L);
int f_gc (lua_State *L);
int f_tostring (lua_State *L);
int io_open (lua_State *L);
int io_read (lua_State *L);
int io_type (lua_State *L);

void createstdfile (lua_State *L, FILE *f, const char *k, const char *fname);