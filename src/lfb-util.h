#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define ROOT "/media/media/"

char *resolvepath(const char* userpath, const char* root, char* resolved);
char *getpath(lua_State *L);
