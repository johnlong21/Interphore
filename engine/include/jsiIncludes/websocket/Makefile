# Makefile to download/compile libwebsockets.a, with or without CMAKE.
TARGET=unix
LWSBASE=src
CFLAGS  = -g -Wall -I$(SOURCEDIR) -I$(TARGET)/ -fsanitize=address

ifeq ($(MINIZ),1)
CFLAGS += -I../miniz
endif

SOURCEDIR=$(LWSBASE)/lib
LWSINT=$(LWSBASE)
BUILDDIR=build/$(TARGET)
LWSLIBNAME=libwebsockets.a
LWSLIBBUILD=$(LWSLIBNAME)
CMAKE=cmake
DOCMAKE=0

LWSLIB=build/$(TARGET)/$(LWSLIBNAME)

SOURCES = $(SOURCEDIR)/base64-decode.c $(SOURCEDIR)/handshake.c $(SOURCEDIR)/libwebsockets.c \
	$(SOURCEDIR)/service.c $(SOURCEDIR)/pollfd.c $(SOURCEDIR)/output.c $(SOURCEDIR)/parsers.c \
	$(SOURCEDIR)/context.c $(SOURCEDIR)/alloc.c $(SOURCEDIR)/header.c $(SOURCEDIR)/client.c \
	$(SOURCEDIR)/client-handshake.c $(SOURCEDIR)/client-parser.c $(SOURCEDIR)/sha-1.c \
	$(SOURCEDIR)/server.c $(SOURCEDIR)/server-handshake.c \
	$(SOURCEDIR)/extension.c $(SOURCEDIR)/extension-permessage-deflate.c

WFILES = $(SOURCEDIR)/lws-plat-win.c 
UFILES = $(SOURCEDIR)/lws-plat-unix.c 

ifeq ($(WIN),1)
CFLAGS +=  -D__USE_MINGW_ANSI_STDIO
SOURCES += $(WFILES)
else
SOURCES += $(UFILES)
endif

OBJLST  = $(SOURCES:.c=.o)
OBJECTS	= $(patsubst $(SOURCEDIR)/%,$(BUILDDIR)/%,$(OBJLST))

USECMAKE=
# Detect cmake and use it if not disabled.
ifeq ($(USECMAKE),)
CMAKEPATH := $(shell which $(CMAKE) )
ifneq ($(CMAKEPATH),)
DOCMAKE=1
endif
endif

LWSFLAGS = -DLWS_WITHOUT_TESTAPPS=1 -DCMAKE_C_FLAGS=-Wno-error -DWITH_SSL=0  \
	-DLWS_WITH_SSL=0 -DLWS_WITH_ZLIB=0 -DLWS_USE_BUNDLED_ZLIB=0 -DLWS_WITHOUT_EXTENSIONS=1

ifeq ($(WIN),1)
LWSFLAGS += -DLWS_USE_BUNDLED_ZLIB=1 -DCMAKE_TOOLCHAIN_FILE=../../$(LWSBASE)/cross-ming.cmake \
	-DCMAKE_INSTALL_PREFIX:PATH=/usr -DLWS_WITH_STATIC=1 -DLWS_WITH_SHARED=0
LWSLIBBUILD=libwebsockets_static.a
endif

all: mkdirs download $(BUILDDIR)/libwebsockets.a

mkdirs:
	mkdir -p $(BUILDDIR)

download: $(SOURCEDIR)/base64-decode.c

$(SOURCEDIR)/base64-decode.c:
	wget http://jsish.org/download/libwebsockets-1.7.zip
	unzip libwebsockets-1.7.zip
	ln -sf libwebsockets-1.7 src
	rm libwebsockets-1.7.zip
	$(MAKE)

ifeq ($(DOCMAKE),1)
$(BUILDDIR)/libwebsockets.a:
	( mkdir -p $(BUILDDIR) && cd $(BUILDDIR) && CC=$(CC) AR=$(AR) $(CMAKE) ../../$(LWSBASE) $(LWSFLAGS)   && $(MAKE) CC=$(CC) AR=$(AR) && cp lib/$(LWSLIBBUILD) libwebsockets.a)
else

$(BUILDDIR)/%.o: $(SOURCEDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS) 

$(BUILDDIR)/libwebsockets.a: $(OBJECTS)
	$(AR) rv $@ $(OBJECTS)

endif

clean:
	rm -f $(OBJECTS) $(BUILDDIR)/libwebsockets.a

cleanall: clean
	rm -rf build
	
.PHONY: all depend remake clean cleanall download
