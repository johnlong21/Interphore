# Makefile for jsish: controlled by make.conf from configure.
PREFIX=/usr/local
WEBSOCKDIR = websocket
WEBSOCKSRC = $(WEBSOCKDIR)/src
SQLITEDIR = sqlite
ACFILES	= parser.c
#ACFILES	= jsiParser.c
BUILDSYS = $(shell uname -o)

CFLAGS += -I. -Wall -Wsign-compare -Wtype-limits -Wuninitialized
# -pg
#CFLAGS += -g -O3
CFLAGS += -g -Og -O0
#CFLAGS += -g -Og -g3

MAKEFILE=Makefile
CMAKE=cmake
USECMAKE=

PCFILES = jsiLexer.c jsiFunc.c jsiValue.c jsiRegexp.c jsiPstate.c jsiInterp.c \
    jsiUtils.c jsiProto.c jsiFilesys.c jsiChar.c jsiString.c jsiBool.c \
    jsiNumber.c jsiArray.c jsiLoad.c jsiHash.c jsiOptions.c jsiStubs.c \
    jsiFormat.c jsiJSON.c jsiCmds.c jsiFileCmds.c jsiObj.c jsiSignal.c\
    jsiTree.c jsiCrypto.c jsiDString.c jsiMath.c jsmn.c jsiZvfs.c jsiUtf8.c jsiUserObj.c\
    jsiSocket.c jsiSqlite.c jsiWebSocket.c jsiMySql.c jsiCData.c jsiMarkdown.c
CFILES = $(PCFILES) $(ACFILES)
EFILES = jsiEval.c
WFILES = win/compat.c win/strptime.c 
WIFILES = win/compat.h
REFILES = regex/regex.h regex/tre.h regex/regcomp.c  regex/regerror.c  regex/regexec.c regex/tre-mem.c
HFILES = parser.h jsiInt.h
PROGRAM=jsish
CONF_ARGS=
MAKECONF=make.conf
-include $(MAKECONF)

ifneq ($(JSI_CONFIG_DEFINED),1)
unconfigured:
	@echo "ERROR!!!!!!!!!  NEED TO RUN: ./configure"
endif

# Detect when config file changed in incompatible way.
EXPECT_CONFIG_VER=2.0323
#ifneq ($(DEFCONFIG_VER),$(EXPECT_CONFIG_VER))
#badconfigured:
#	mv -f make.conf make.conf.bak
#	@echo "ERROR!!!!!!!!!  renamed incompatiable config file make.conf: Please rerun ./configure"
#	@echo "<$(DEFCONFIG_VER) != $(EXPECT_CONFIG_VER)>"
#endif

# Detect when jsimin is downlevel
#ifneq ($(CHECK_CONFIG_VER),)
#chkconfig:
#ifneq ($(CHECK_CONFIG_VER),$(EXPECT_CONFIG_VER))
#badjsimin:
#	mv -f jsimin jsimin.bak
#	@echo "NOTE!!!!!!!!!  renamed incompatiable jsimin"
#	@echo "<$(CHECK_CONFIG_VER) != $(EXPECT_CONFIG_VER)>"
#endif
#endif


ifeq ($(TARGET),win)
	JSI__REGEX=1
endif

ifeq ($(JSI__REGEX),1)
CFLAGS += -Iregex
CFILES += regex/regcomp.c  regex/regerror.c  regex/regexec.c regex/tre-mem.c
#HFILES += regex/regex.h regex/tre.h
endif

ifeq ($(LINKSTATIC),1)
PROGLDFLAGS += -static
endif

ifeq ($(JSI__SANITIZE),1)
CFLAGS += -fsanitize=address
ASAN_OPTIONS=abort_on_error=1
endif

ifeq ($(WITH_EXT_WEBSOCKET),1)

ifeq ($(BUILDIN_WEBSOCKET),1)
WEBSOCKLIB = $(WEBSOCKDIR)/build/$(TARGET)/libwebsockets.a
CFLAGS += -I$(WEBSOCKSRC)/lib -I$(WEBSOCKSRC)/build -Iwebsocket/$(TARGET)
STATICLIBS += $(WEBSOCKLIB)

else
WEBSOCKLIB = -lwebsockets
endif

PROGFLAGS += -DJSI__WEBSOCKET=1
else
endif

ifeq ($(WITH_EXT_SQLITE),1)

ifeq ($(BUILDIN_SQLITE),1)
SQLITELIB = $(SQLITEDIR)/build/$(TARGET)/libsqlite3.a
CFLAGS += -I$(SQLITEDIR)/src
else
SQLITELIB = -lsqlite3
endif

PROGFLAGS += -DJSI__SQLITE=1
ifneq ($(ISSHARED),1)
endif
ifeq ($(DB_TEST),1)
PROGFLAGS += -DJSI_DB_TEST=1
endif
endif

ifeq ($(WITH_EXT_MYSQL),1)
PROGLDFLAGS += -lmysqlclient
PROGFLAGS += -DJSI__MYSQL=1
endif

BUILDDIR = $(PWD)

ifneq ($(EXTNAME),)
CFILES += $(EXTNAME).c
PROGFLAGS += -DJSI_USER_EXTENSION=Jsi_Init$(EXTNAME)
endif

ifeq ($(TARGET),win)
# *********** WINDOWS *****************
WIN=1
CFLAGS += -D__USE_MINGW_ANSI_STDIO=1 -Wno-format

# Setup cross-compiler for windows.
ifeq ($(XCPREFIX),)
XCPREFIX=i686-w64-mingw32-
TCPATH := $(shell which $(XCPREFIX)gcc )
ifeq ($(TCPATH),)
XCPREFIX=x86_64-w64-mingw32-
endif
endif

OPT_SOCKET=0
OPT_READLINE=0

EXEEXT=.exe
CFILES += $(WFILES) $(SQLITEDIR)/src/sqlite3.c

ifneq ($(WITH_EXT_WEBSOCKET),1)
#for windows without websock use miniz

ifneq ($(JSI__MINIZ),1)
JSI__MINIZ=1
PROGFLAGS += JSI__MINIZ=1
endif

else
WEBSOCKLIB=$(WEBSOCKDIR)/build/$(TARGET)/libwebsockets.a
PROGLDFLAGS += $(WEBSOCKLIB) -lwsock32 -lws2_32
endif

STATICLIBS += $(SQLITELIB) $(WEBSOCKLIB)

ifeq ($(JSI__THREADS),1)
PROGLDFLAGS += -lpthread -static
endif

# ***** END WINDOWS *********

else
# *********** UNIX **********************

ifneq ($(BUILDSYS),FreeBSD)
CFLAGS += -frecord-gcc-switches
endif
STATICLIBS += $(SQLITELIB)
PROGLDFLAGS += $(WEBSOCKLIB)

ifeq ($(JSI__LOAD),1)
LNKFLAGS += -rdynamic
COPTS = -fpic
ifneq ($(BUILDSYS),FreeBSD)
PROGLDFLAGS += -ldl
endif
endif

ifeq ($(JSI__THREADS),1)
PROGLDFLAGS += -lpthread
endif

ifneq ($(TARGET),musl)
endif

endif
# *********** END UNIX **********************
ifeq ($(BUILDSYS),FreeBSD)
CC=$(XCPREFIX)cc
else
CC=$(XCPREFIX)gcc
endif
AR=$(XCPREFIX)ar
LD=$(XCPREFIX)ld

ifeq ($(TARGET),musl)
PROGFLAGS += -DJSI__MUSL
CC=musl-gcc
endif

CCPATH := $(shell which $(CC) )
ifeq ($(CCPATH),)
error:
	@echo "ERROR: Compiler not found '$(CC)': try setting with configure --xcprefix"
endif

ifeq ($(JSI__READLINE),1)
CFILES += linenoise.c
HFILES += linenoise.h
ifeq ($(JSI__GNUREADLINE),1)
PROGLDFLAGS += -lreadline -lncurses
endif
endif

MINIZDIR=miniz
ifeq ($(JSI__MINIZ),1)
CFILES += $(MINIZDIR)/miniz.c
CFLAGS += -I$(PWD)/$(MINIZDIR)
else
ifneq ($(WIN),1)
PROGLDFLAGS += -lz
endif
endif

PROGLDFLAGS += $(USERLIB)
OBJS    = $(CFILES:.c=.o) $(EFILES:.c=.o)
DEFIN	= 
CFLAGS	+= $(COPTS) $(DEFIN) $(PROGFLAGS) $(OPTS)
YACC	= bison -v
LDFLAGS = -lm $(PROGLDFLAGS)
SHLEXT=.so

ZIPDIR=zipdir
BLDDIR=$(PWD)
PROGBINMIN = ./jsimin
PROGBINA   = $(PROGRAM)_$(EXEEXT)
PROGBIN	   = $(PROGRAM)$(EXEEXT)

JSI_PKG_DIRS="$(BLDDIR)/lib,$(PREFIX)/lib/jsi"
CFLAGS += -DJSI_PKG_DIRS=\"$(JSI_PKG_DIRS)\"
CFLAGS += -DJSI_CONF_ARGS=\"$(CONF_ARGS)\"

#.PHONY: all clean cleanall remake

all: $(STATICLIBS) $(PROGBIN) checkcfgver

help:
	@echo "targets are: mkwin mkmusl shared jsishs stubs ref test testmem release"

main.o: .FORCE

.FORCE:

$(OBJS) : $(MAKEFILE) $(MAKECONF)

libjsi.a:
	$(AR) r libjsi.a $(OBJS)

modules: $(BUILDMODS)

$(PROGBINA): parser.c $(OBJS) main.o libjsi.a
	$(AR) r libjsi.a $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(SQLITELIB) main.o $(LNKFLAGS) -o $(PROGBINA) $(LDFLAGS)
	$(MAKE) modules

jsishs$(EXEEXT): parser.c $(OBJS) main.o
	$(CC) $(CFLAGS) main.o -o $@ -L. -ljsi $(LDFLAGS) -lsqlite3 $(LOADLIB) $(THREADLIB)

shared: libjsi$(SHLEXT) jsish$(EXEEXT)

$(PROGBIN): $(PROGBINA)  .FORCE
ifneq ($(JSI__ZIPLIB),1)
	cp -f $(PROGBINA) $(PROGBIN)
else
	rm -f $@
	cp $(PROGBINA) $@
ifneq ($(wildcard ../.fslckout),) 
	fossil info | grep ^checkout | cut -b15- > lib/sourceid.txt
endif
	$(PROGBINMIN) lib/Jsi_Zip.jsi create $@ $(ZIPDIR) lib
endif
	@echo "Congratulations, build is now complete. Test with './jsish -u tests'"

apps: ledger.zip
#apps: sqliteui$(EXEEXT)

ledger.zip: .FORCE
	rm $@
	(cd ../Ledger && zip -r  ../jsi/$@ .)


sqliteui$(EXEEXT):  .FORCE
	cp $(PROGBINA) $@
	$(PROGBINMIN) lib/Jsi_Zip.jsi create  $@ ../sqliteui lib

libwebsocket: $(WEBSOCKLIB)
USECMAKE=0

$(WEBSOCKLIB):
	$(MAKE) -C $(WEBSOCKDIR) CC=$(CC) AR=$(AR) WIN=$(WIN) TARGET=$(TARGET) USECMAKE=$(USECMAKE) MINIZ=$(JSI__MINIZ)

$(SQLITELIB):
	$(MAKE) -C $(SQLITEDIR) CC=$(CC) AR=$(AR) LD=$(LD) WIN=$(WIN) TARGET=$(TARGET)


%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 


.FORCE:

depend:
	$(CC) -E -MM -DJSI__WEBSOCKET=1 -DJSI__SQLITE -DJSI__MYSQL $(CFLAGS) $(CFILES) $(EFILES) > .depend

-include .depend

libjsi$(SHLEXT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -Wl,--export-dynamic  -shared -o $@ $(LDFLAGS)

# Supported modules (unix only)
mysql: MySql$(SHLEXT)
sqlite: Sqlite$(SHLEXT)
websocket: WebSocket$(SHLEXT)

MySql$(SHLEXT): jsiMySql.c
	-$(CC) $(CFLAGS) $(PROGFLAGS) $(MODFLAGS) jsiMySql.c -Wl,--export-dynamic  -shared -o $@ $(LDFLAGS) -lmysqlclient

Sqlite$(SHLEXT): jsiSqlite.c
	-$(CC) $(CFLAGS) $(PROGFLAGS) $(MODFLAGS)  jsiSqlite.c -Wl,--export-dynamic  -shared -o $@ $(LDFLAGS) -lsqlite3
	
WebSocket$(SHLEXT): jsiWebSocket.c
	-$(CC) $(CFLAGS) $(PROGFLAGS) $(MODFLAGS)  jsiWebSocket.c -Wl,--export-dynamic  -shared -o $@ $(LDFLAGS) -lwebsockets

parser.c: parser.y
	$(YACC) -oparser.c -d parser.y

jsiParser.c: jsiParser.y
	-lemon jsiParser.y

# Create the single amalgamation file jsi.c
jsi.c: jsi.h $(REFILES) $(HFILES) $(CFILES) $(MAKEFILE) $(MAKECONF)
	cat jsi.h > $@
	echo "#ifndef JSI_IN_AMALGAMATION" >> $@
	echo "#define JSI_IN_AMALGAMATION" >> $@
	echo "#define _GNU_SOURCE"  >> $@
	echo "#define JSI_AMALGAMATION" >> $@
	echo "#if JSI__MINIZ==1" >> $@
	cat $(MINIZDIR)/miniz.c >> $@
	echo "#endif //JSI__MINIZ==1 " >> $@
	echo "#if JSI__READLINE==1" >> $@
	cat linenoise.c >> $@
	echo "#endif //JSI__READLINE==1" >> $@
	echo "struct jsi_Pstate;" >> $@
	cat $(WIFILES) $(REFILES) $(HFILES) jsiCode.c $(PCFILES) | grep -v '^#line' >> $@
	echo "#ifndef JSI_LITE_ONLY" >> $@
	grep -v '^#line' $(ACFILES)  >> $@
	echo "#endif //JSI_LITE_ONLY" >> $@
	cat $(WFILES) $(EFILES)  >> $@
	echo "#if JSI__MAIN==1" >> $@
	cat main.c  >> $@
	echo "#endif //JSI__MAIN==1" >> $@
	echo "#endif //JSI_IN_AMALGAMATION" >> $@

# Create the single compile file jsiOne.c
jsiOne.c: jsi.h  $(REFILES) $(HFILES) $(CFILES) $(MAKEFILE) $(MAKECONF)
	echo '#include "jsi.h"' > $@
	echo "#define JSI_AMALGAMATION" >> $@
	echo "#if JSI__MINIZ==1" >> $@
	echo '#include "'$(MINIZDIR)/miniz.c'"' >> $@
	echo "#endif" >> $@
	echo "#if JSI__READLINE==1" >> $@
	echo '#include "'linenoise.c'"' >> $@
	echo "#endif" >> $@
	echo "struct jsi_Pstate;" >> $@
	for ii in  $(REFILES) $(HFILES) jsiCode.c $(PCFILES); do echo '#include "'$$ii'"' >> $@; done
	echo "#ifndef JSI_LITE_ONLY" >> $@
	for ii in $(ACFILES); do echo '#include "'$$ii'"' >> $@; done
	echo "#endif" >> $@
	for ii in $(WFILES) $(EFILES); do echo '#include "'$$ii'"' >> $@; done
	echo "#if JSI__MAIN==1" >> $@
	echo '#include "main.c"'  >> $@
	echo "#endif" >> $@

stubs:
	./$(PROGBIN) tools/mkstubs.jsi

ref:
	./$(PROGBIN) tools/mkproto.jsi > tools/protos.jsi
	$(MAKE) -C www

release: stubs ref jsi.c jsiOne.c

printconf:
	@echo $(EXPECT_CONFIG_VER)

test:
	./jsish -u tests
#	tools/testjs.sh tests

# This requires building with memdebug.
testmem:
	JSI_INTERP_OPTS='{memDebug:1}' ./jsish -u tests

testwall:
	JSI_INTERP_OPTS='{typeCheck:["strict"], strict:true}' ./jsish -u tests

testvg:
	tools/testjs.sh -jsish ./jsish -valgrind tests

tags:
	geany -g -P geany.tags *.c *.h

install: all
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/lib/jsi
	mkdir -p $(PREFIX)/include
	cp jsish $(PREFIX)/bin
	cp jsimin $(PREFIX)/bin
	cp tools/jsish-c $(PREFIX)/bin
	cp jsi.h $(PREFIX)/include
	cp jsiStubs.h $(PREFIX)/include
	-cp Sqlite$(SHLEXT) $(PREFIX)/lib/jsi
	-cp WebSocket$(SHLEXT) $(PREFIX)/lib/jsi
	-cp Socket$(SHLEXT) $(PREFIX)/lib/jsi
	-cp MySql$(SHLEXT) $(PREFIX)/lib/jsi

setup:
	egrep -s 'complete -f '.*jsish' ~/.bashrc || echo "complete -W '-h --help -v --version -d --debug -D --debugui -u --unittest -s --safe -Z --zip -S --sqliteui -W --websrv -H --htmli -J --jsi -c --cdata -C --cssi -m --module -e --eval -t --tracecall -T --typecheck -t --tracecall' -f '*.jsi'  'jsish'\n" >> ~/.bashrc
remake: clean all

clean:
	rm -rf *.o *.a jsish $(MINIZDIR)/*.o win/*.o regex/*.o
	$(MAKE) -C sqlite clean
	$(MAKE) -C websocket clean

cleanall: clean
	rm -f $(ACFILES) $(PROGBINA) core parser.c parser.h parser.tab.c jsimin jsish *.so $(PROGBINMIN)
	$(MAKE) -C sqlite cleanall
	$(MAKE) -C websocket cleanall

JSIMINVER=$(shell test -x ./jsimin && ./jsimin -v | cut -d' ' -f2)
JSICURVER=$(shell fgrep 'define JSI_VERSION_' jsi.h | cut -b29- | xargs | sed 's/ /./g')
CURCONFVER=$(shell test -f make.conf && fgrep DEFCONFIG_VER make.conf | cut -d= -f2)

checkjsiminver:
ifneq ($(JSIMINVER), $(JSICURVER))
	@echo "jsimin version mismatch"
	rm -f jsimin
endif

checkcfgver:
ifneq ($(CURCONFVER), $(JSICURVER))
	@echo "NOTE: version changed since last run of configure: $(CURCONFVER) != $(JSICURVER)"
endif

