# $Id: Makefile,v 1.36 2009/09/21 17:02:44 mascarenhas Exp $

CONFIG= ./config

include $(CONFIG)

SRCS= src/*.c
OBJS= src/*.o

lib: src/lfs.so

src/lfs.so: $(OBJS)
	MACOSX_DEPLOYMENT_TARGET="10.3"; export MACOSX_DEPLOYMENT_TARGET; $(CC) $(LIB_OPTION) -o src/lfb.so $(SRCS)

test: lib
	LUA_CPATH=./src/?.so lua tests/test.lua

install:
	mkdir -p $(LUA_LIBDIR)
	cp src/lfs.so $(LUA_LIBDIR)

clean:
	rm -f src/lfs.so $(OBJS)
