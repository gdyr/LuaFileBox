#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "lfb-util.h"

/*
** This function resolves a relative path to an absolute path, and checks that it is contained within the root.
*/
char *resolvepath(const char* userpath, const char* root, char* resolved) {

  char prefixedpath[1024];
  char *resolvedptr;

  // Prepend path prefix
  strcpy(prefixedpath, root);
  strcat(prefixedpath, userpath);

  // Resolve path
  resolvedptr = realpath(prefixedpath, resolved); // Convert to absolute path
  if(resolvedptr == NULL) { return NULL; }    // Cascade resolution errors

  // Check whether the path is still contained
  if(strncmp(resolvedptr, root, strlen(root)-1) != 0) {
    errno = ENOENT;
    return NULL;
  }

  return resolvedptr;

}

char *getpath(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  char resolved[1024];
  char *resolvedptr = resolvepath(path, ROOT, resolved);
  if(resolvedptr == NULL)
    luaL_error (L, "cannot open %s: %s", path, strerror (errno));
  return resolvedptr;
}
