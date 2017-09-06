/*
** LuaFileBox
** Copyright 2017 Michael Goodyear
** Based on LuaFileSystem, (C) Kepler Project 2003 - 2016 (http://keplerproject.github.io/luafilesystem)
**
** File system manipulation library.
** This library offers these functions:
**   Files.attributes (filepath [, attributename | attributetable])
**   Files.chdir (path)
**   Files.currentdir ()
**   Files.dir (path)
**   REM: Files.link (old, new[, symlink])
**   REM: Files.lock (fh, mode)
**   REM: Files.lock_dir (path)
**   Files.mkdir (path)
**   Files.rmdir (path)
**   REM: Files.setmode (filepath, mode)
**   REM: Files.symlinkattributes (filepath [, attributename])
**   Files.touch (filepath [, atime [, mtime]])
**   Files.lines (fh)
**   Files.open (fh)
**   Files.type (fh)
**   file:close (fh)
**   file:flush (fh)
**   file:lines (fh)
**   file:read (fh)
**   file:seek (fh)
**   file:setvbuf (fh)
**   file:write (fh)
*/

#ifndef LFS_DO_NOT_USE_LARGE_FILE
#ifndef _WIN32
#ifndef _AIX
#define _FILE_OFFSET_BITS 64 /* Linux, Solaris and HP-UX */
#else
#define _LARGE_FILES 1 /* AIX */
#endif
#endif
#endif

#ifndef LFS_DO_NOT_USE_LARGE_FILE
#define _LARGEFILE64_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #include <windows.h>
  #include <io.h>
  #include <sys/locking.h>
  #ifdef __BORLANDC__
    #include <utime.h>
  #else
    #include <sys/utime.h>
  #endif
  #include <fcntl.h>
  /* MAX_PATH seems to be 260. Seems kind of small. Is there a better one? */
  #define LFS_MAXPATHLEN MAX_PATH
#else
  #include <unistd.h>
  #include <dirent.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <utime.h>
  #include <sys/param.h> /* for MAXPATHLEN */
  #define LFS_MAXPATHLEN MAXPATHLEN
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "lfs.h"

#include "lfb-util.h"

#define LFS_VERSION "1.6.3"
#define LFS_LIBNAME "lfs"

#if LUA_VERSION_NUM >= 503 /* Lua 5.3 */

#ifndef luaL_optlong
#define luaL_optlong luaL_optinteger
#endif

#endif

#if LUA_VERSION_NUM < 502
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

/* Define 'strerror' for systems that do not implement it */
#ifdef NO_STRERROR
#define strerror(_)     "System unable to describe the error"
#endif

#define DIR_METATABLE "directory metatable"
typedef struct dir_data {
        int  closed;
#ifdef _WIN32
        intptr_t hFile;
        char pattern[MAX_PATH+1];
#else
        DIR *dir;
#endif
} dir_data;

#define LOCK_METATABLE "lock metatable"

#ifdef _WIN32
 #ifdef __BORLANDC__
  #define lfs_setmode(file, m)   (setmode(_fileno(file), m))
  #define STAT_STRUCT struct stati64
 #else
  #define lfs_setmode(file, m)   (_setmode(_fileno(file), m))
  #define STAT_STRUCT struct _stati64
 #endif
#define STAT_FUNC _stati64
#define LSTAT_FUNC STAT_FUNC
#else
#define _O_TEXT               0
#define _O_BINARY             0
#define lfs_setmode(file, m)   ((void)file, (void)m, 0)
#define STAT_STRUCT struct stat
#define STAT_FUNC stat
#define LSTAT_FUNC lstat
#endif

/*
** Utility functions
*/
static int pusherror(lua_State *L, const char *info)
{
        lua_pushnil(L);
        if (info==NULL)
                lua_pushstring(L, strerror(errno));
        else
                lua_pushfstring(L, "%s: %s", info, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
}


/*
** This function changes the working (current) directory
*/
int change_dir (lua_State *L) {
        const char *path = getpath(L);
        if (chdir(path)) {
                lua_pushnil (L);
                lua_pushfstring (L,"Unable to change working directory to '%s'\n%s\n",
                                path, chdir_error);
                return 2;
        } else {
                lua_pushboolean (L, 1);
                return 1;
        }
}

/*
** This function returns the current directory
** If unable to get the current directory, it returns nil
**  and a string describing the error
*/
int get_dir (lua_State *L) {
#ifdef NO_GETCWD
    lua_pushnil(L);
    lua_pushstring(L, "Function 'getcwd' not provided by system");
    return 2;
#else
    char *path = NULL;
    /* Passing (NULL, 0) is not guaranteed to work. Use a temp buffer and size instead. */
    size_t size = LFS_MAXPATHLEN; /* initial buffer size */
    int result;
    while (1) {
        path = realloc(path, size);
        if (!path) /* failed to allocate */
            return pusherror(L, "get_dir realloc() failed");
        if (getcwd(path, size) != NULL) {
            /* success, push the path to the Lua stack */
            lua_pushstring(L, path);
            result = 1;
            break;
        }
        if (errno != ERANGE) { /* unexpected error */
            result = pusherror(L, "get_dir getcwd() failed");
            break;
        }
        /* ERANGE = insufficient buffer capacity, double size and retry */
        size *= 2;
    }
    free(path);
    return result;
#endif
}

/*
** Check if the given element on the stack is a file and returns it.
*/
static FILE *check_file (lua_State *L, int idx, const char *funcname) {
#if LUA_VERSION_NUM == 501
        FILE **fh = (FILE **)luaL_checkudata (L, idx, "FILE*");
        if (*fh == NULL) {
                luaL_error (L, "%s: closed file", funcname);
                return 0;
        } else
                return *fh;
#elif LUA_VERSION_NUM >= 502 && LUA_VERSION_NUM <= 503
        luaL_Stream *fh = (luaL_Stream *)luaL_checkudata (L, idx, "FILE*");
        if (fh->closef == 0 || fh->f == NULL) {
                luaL_error (L, "%s: closed file", funcname);
                return 0;
        } else
                return fh->f;
#else
#error unsupported Lua version
#endif
}

/*
** Creates a directory.
** @param #1 Directory path.
*/
int make_dir (lua_State *L) {
        const char *path = getpath(L);
        int fail;
#ifdef _WIN32
        fail = _mkdir (path);
#else
        fail =  mkdir (path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP |
                             S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH );
#endif
        if (fail) {
                lua_pushnil (L);
        lua_pushfstring (L, "%s", strerror(errno));
                return 2;
        }
        lua_pushboolean (L, 1);
        return 1;
}


/*
** Removes a directory.
** @param #1 Directory path.
*/
int remove_dir (lua_State *L) {
        const char *path = getpath(L);
        int fail;

        fail = rmdir (path);

        if (fail) {
                lua_pushnil (L);
                lua_pushfstring (L, "%s", strerror(errno));
                return 2;
        }
        lua_pushboolean (L, 1);
        return 1;
}


/*
** Directory iterator
*/
static int dir_iter (lua_State *L) {
#ifdef _WIN32
        struct _finddata_t c_file;
#else
        struct dirent *entry;
#endif
        dir_data *d = (dir_data *)luaL_checkudata (L, 1, DIR_METATABLE);
        luaL_argcheck (L, d->closed == 0, 1, "closed directory");
#ifdef _WIN32
        if (d->hFile == 0L) { /* first entry */
                if ((d->hFile = _findfirst (d->pattern, &c_file)) == -1L) {
                        lua_pushnil (L);
                        lua_pushstring (L, strerror (errno));
                        d->closed = 1;
                        return 2;
                } else {
                        lua_pushstring (L, c_file.name);
                        return 1;
                }
        } else { /* next entry */
                if (_findnext (d->hFile, &c_file) == -1L) {
                        /* no more entries => close directory */
                        _findclose (d->hFile);
                        d->closed = 1;
                        return 0;
                } else {
                        lua_pushstring (L, c_file.name);
                        return 1;
                }
        }
#else
        if ((entry = readdir (d->dir)) != NULL) {
                lua_pushstring (L, entry->d_name);
                return 1;
        } else {
                /* no more entries => close directory */
                closedir (d->dir);
                d->closed = 1;
                return 0;
        }
#endif
}


/*
** Closes directory iterators
*/
static int dir_close (lua_State *L) {
        dir_data *d = (dir_data *)lua_touserdata (L, 1);
#ifdef _WIN32
        if (!d->closed && d->hFile) {
                _findclose (d->hFile);
        }
#else
        if (!d->closed && d->dir) {
                closedir (d->dir);
        }
#endif
        d->closed = 1;
        return 0;
}


/*
** Factory of directory iterators
*/
int dir_iter_factory (lua_State *L) {
        const char *path = getpath(L);
        dir_data *d;
        lua_pushcfunction (L, dir_iter);
        d = (dir_data *) lua_newuserdata (L, sizeof(dir_data));
        luaL_getmetatable (L, DIR_METATABLE);
        lua_setmetatable (L, -2);
        d->closed = 0;
#ifdef _WIN32
        d->hFile = 0L;
        if (strlen(path) > MAX_PATH-2)
          luaL_error (L, "path too long: %s", path);
        else
          sprintf (d->pattern, "%s/*", path);
#else
        d->dir = opendir (path);
        if (d->dir == NULL)
          luaL_error (L, "cannot open %s: %s", path, strerror (errno));
#endif
        return 2;
}


/*
** Creates directory metatable.
*/
int dir_create_meta (lua_State *L) {
        luaL_newmetatable (L, DIR_METATABLE);

        /* Method table */
        lua_newtable(L);
        lua_pushcfunction (L, dir_iter);
        lua_setfield(L, -2, "next");
        lua_pushcfunction (L, dir_close);
        lua_setfield(L, -2, "close");

        /* Metamethods */
        lua_setfield(L, -2, "__index");
        lua_pushcfunction (L, dir_close);
        lua_setfield (L, -2, "__gc");
        return 1;
}


#ifdef _WIN32
 #ifndef S_ISDIR
   #define S_ISDIR(mode)  (mode&_S_IFDIR)
 #endif
 #ifndef S_ISREG
   #define S_ISREG(mode)  (mode&_S_IFREG)
 #endif
 #ifndef S_ISLNK
   #define S_ISLNK(mode)  (0)
 #endif
 #ifndef S_ISSOCK
   #define S_ISSOCK(mode)  (0)
 #endif
 #ifndef S_ISFIFO
   #define S_ISFIFO(mode)  (0)
 #endif
 #ifndef S_ISCHR
   #define S_ISCHR(mode)  (mode&_S_IFCHR)
 #endif
 #ifndef S_ISBLK
   #define S_ISBLK(mode)  (0)
 #endif
#endif
/*
** Convert the inode protection mode to a string.
*/
#ifdef _WIN32
static const char *mode2string (unsigned short mode) {
#else
static const char *mode2string (mode_t mode) {
#endif
  if ( S_ISREG(mode) )
    return "file";
  else if ( S_ISDIR(mode) )
    return "directory";
  else if ( S_ISLNK(mode) )
        return "link";
  else if ( S_ISSOCK(mode) )
    return "socket";
  else if ( S_ISFIFO(mode) )
        return "named pipe";
  else if ( S_ISCHR(mode) )
        return "char device";
  else if ( S_ISBLK(mode) )
        return "block device";
  else
        return "other";
}


/*
** Set access time and modification values for file
*/
int file_utime (lua_State *L) {
        const char *file = getpath(L);
        struct utimbuf utb, *buf;

        if (lua_gettop (L) == 1) /* set to current date/time */
                buf = NULL;
        else {
                utb.actime = (time_t)luaL_optnumber (L, 2, 0);
                utb.modtime = (time_t) luaL_optinteger (L, 3, utb.actime);
                buf = &utb;
        }
        if (utime (file, buf)) {
                lua_pushnil (L);
                lua_pushfstring (L, "%s", strerror (errno));
                return 2;
        }
        lua_pushboolean (L, 1);
        return 1;
}


/* inode protection mode */
static void push_st_mode (lua_State *L, STAT_STRUCT *info) {
        lua_pushstring (L, mode2string (info->st_mode));
}
/* device inode resides on */
static void push_st_dev (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer) info->st_dev);
}
/* inode's number */
static void push_st_ino (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer) info->st_ino);
}
/* number of hard links to the file */
static void push_st_nlink (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer)info->st_nlink);
}
/* user-id of owner */
static void push_st_uid (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer)info->st_uid);
}
/* group-id of owner */
static void push_st_gid (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer)info->st_gid);
}
/* device type, for special file inode */
static void push_st_rdev (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer) info->st_rdev);
}
/* time of last access */
static void push_st_atime (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer) info->st_atime);
}
/* time of last data modification */
static void push_st_mtime (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer) info->st_mtime);
}
/* time of last file status change */
static void push_st_ctime (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer) info->st_ctime);
}
/* file size, in bytes */
static void push_st_size (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer)info->st_size);
}
#ifndef _WIN32
/* blocks allocated for file */
static void push_st_blocks (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer)info->st_blocks);
}
/* optimal file system I/O blocksize */
static void push_st_blksize (lua_State *L, STAT_STRUCT *info) {
        lua_pushinteger (L, (lua_Integer)info->st_blksize);
}
#endif

 /*
** Convert the inode protection mode to a permission list.
*/

#ifdef _WIN32
static const char *perm2string (unsigned short mode) {
  static char perms[10] = "---------";
  int i;
  for (i=0;i<9;i++) perms[i]='-';
  if (mode  & _S_IREAD)
   { perms[0] = 'r'; perms[3] = 'r'; perms[6] = 'r'; }
  if (mode  & _S_IWRITE)
   { perms[1] = 'w'; perms[4] = 'w'; perms[7] = 'w'; }
  if (mode  & _S_IEXEC)
   { perms[2] = 'x'; perms[5] = 'x'; perms[8] = 'x'; }
  return perms;
}
#else
static const char *perm2string (mode_t mode) {
  static char perms[10] = "---------";
  int i;
  for (i=0;i<9;i++) perms[i]='-';
  if (mode & S_IRUSR) perms[0] = 'r';
  if (mode & S_IWUSR) perms[1] = 'w';
  if (mode & S_IXUSR) perms[2] = 'x';
  if (mode & S_IRGRP) perms[3] = 'r';
  if (mode & S_IWGRP) perms[4] = 'w';
  if (mode & S_IXGRP) perms[5] = 'x';
  if (mode & S_IROTH) perms[6] = 'r';
  if (mode & S_IWOTH) perms[7] = 'w';
  if (mode & S_IXOTH) perms[8] = 'x';
  return perms;
}
#endif

/* permssions string */
static void push_st_perm (lua_State *L, STAT_STRUCT *info) {
    lua_pushstring (L, perm2string (info->st_mode));
}

typedef void (*_push_function) (lua_State *L, STAT_STRUCT *info);

struct _stat_members {
        const char *name;
        _push_function push;
};

struct _stat_members members[] = {
        { "mode",         push_st_mode },
        { "dev",          push_st_dev },
        { "ino",          push_st_ino },
        { "nlink",        push_st_nlink },
        { "uid",          push_st_uid },
        { "gid",          push_st_gid },
        { "rdev",         push_st_rdev },
        { "access",       push_st_atime },
        { "modification", push_st_mtime },
        { "change",       push_st_ctime },
        { "size",         push_st_size },
        { "permissions",  push_st_perm },
#ifndef _WIN32
        { "blocks",       push_st_blocks },
        { "blksize",      push_st_blksize },
#endif
        { NULL, NULL }
};

/*
** Get file or symbolic link information
*/
static int _file_info_ (lua_State *L, int (*st)(const char*, STAT_STRUCT*)) {
        STAT_STRUCT info;
        const char *file = getpath(L);
        int i;

        if (st(file, &info)) {
                lua_pushnil(L);
                lua_pushfstring(L, "cannot obtain information from file '%s': %s", file, strerror(errno));
                return 2;
        }
        if (lua_isstring (L, 2)) {
                const char *member = lua_tostring (L, 2);
                for (i = 0; members[i].name; i++) {
                        if (strcmp(members[i].name, member) == 0) {
                                /* push member value and return */
                                members[i].push (L, &info);
                                return 1;
                        }
                }
                /* member not found */
                return luaL_error(L, "invalid attribute name '%s'", member);
        }
        /* creates a table if none is given, removes extra arguments */
        lua_settop(L, 2);
        if (!lua_istable (L, 2)) {
                lua_newtable (L);
        }
        /* stores all members in table on top of the stack */
        for (i = 0; members[i].name; i++) {
                lua_pushstring (L, members[i].name);
                members[i].push (L, &info);
                lua_rawset (L, -3);
        }
        return 1;
}


/*
** Get file information using stat.
*/
int file_info (lua_State *L) {
        return _file_info_ (L, STAT_FUNC);
}
