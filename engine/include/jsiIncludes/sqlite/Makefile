# Makefile to download/compile libsqlite.a
TARGET=unix
SOURCEDIR=src
CFLAGS  = -I$(SOURCEDIR) -I$(LWSBASE)/build -I../miniz -I$(TARGET)/

BUILDDIR=build/$(TARGET)
LIBNAME=libsqlite3.a
LWSLIBBUILD=$(LIBNAME)
CMAKE=cmake

LWSLIB=build/$(TARGET)/$(LIBNAME)

SOURCES = $(SOURCEDIR)/sqlite3.c

OBJLST  = $(SOURCES:.c=.o)
OBJECTS	= $(patsubst $(SOURCEDIR)/%,$(BUILDDIR)/%,$(OBJLST))

all: mkdirs download $(BUILDDIR)/libsqlite3.a

mkdirs:
	mkdir -p $(BUILDDIR)

download: $(SOURCEDIR)/sqlite3.c

$(SOURCEDIR)/sqlite3.c:
	wget https://sqlite.org/2017/sqlite-amalgamation-3180000.zip || wget http://jsish.org/download/sqlite-amalgamation-3180000.zip
	unzip sqlite-amalgamation-3180000.zip
	ln -sf sqlite-amalgamation-3180000 src
	rm sqlite-amalgamation-3180000.zip


$(BUILDDIR)/libsqlite3.a: $(SOURCEDIR)/sqlite3.c
	$(CC) -c -I$(SOURCEDIR) $(SOURCEDIR)/sqlite3.c
	$(AR) rv $(BUILDDIR)/libsqlite3.a sqlite3.o

clean:
	rm -f $(OBJECTS) $(BUILDDIR)/libsqlite3.a

cleanall: clean
	rm -rf build
	
.PHONY: all depend remake clean cleanall download
