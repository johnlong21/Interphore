CFLAGS=-g -Og -O0 -Wall -fPIC -I../.. -DJSI__SQLITE=1 -DJSI__READLINE=1 -DJSI__MEMDEBUG=1 \
	-fno-diagnostics-show-caret -Wc++-compat -Wwrite-strings
# -fsanitize=address
LDFLAGS=-lpthread -ldl -lm -lz -lsqlite3
LDEXT=.so
OTHER=
CSTRPP=../../lib/Jsi_CData.jsi 
DEPS=$(CSTRPP) ../../jsi.c Makefile
CSTRCMD=jsish $(CSTRPP)
CFILES=demo0.h demo1.h demo2.h demo.c
SHFLAGS= -DCDATA_SHARED=1 -shared
#-DJSI_USE_STUBS=1 

ifeq ($(SHLIB),1)
	CFLAGS+= $(SHFLAGS)
	EXT=$(LDEXT)
endif

#CFILES=demo0.h demo2.h demo.c
#CSTRCMD=jsish --cdata

demo: $(CFILES) $(DEPS)
	$(CC) $(CFLAGS) -o $@ demo.c $(LDFLAGS)

demo.so: demo1.h demo2.h demo.c $(DEPS)
	$(CC) $(CFLAGS) $(SHFLAGS) -o $@ demo.c $(LDFLAGS)

all: Bee$(EXT) Car$(EXT) Baker$(EXT)

shared:
	$(MAKE) all SHLIB=1

test: demo
	jsish -u cdatatest.jsi
#	../../tools/testjs.sh -jsish ./demo demotest.jsi

Bee$(EXT): Bee.h $(DEPS)
	$(CC) $(CFLAGS) -DCDATA_MAIN=1 -x c -o $@ Bee.h $(LDFLAGS)

Car$(EXT): Car.h $(DEPS)
	$(CC) $(CFLAGS) -DCDATA_MAIN=1 -x c -o $@ Car.h $(LDFLAGS)

Baker$(EXT): Baker.h $(DEPS)
	$(CC) $(CFLAGS) -DCDATA_MAIN=1 -x c -o $@ Baker.h $(LDFLAGS)
	$(CC) $(CFLAGS) -DCDATA_MAIN=1 -x c -o $@ Baker.h $(LDFLAGS)

%.h: %.jsc $(DEPS)
	$(CSTRCMD) -outFile $@ $<

clean:
	rm -f demo1.h demo2.h demo

depend:
	$(CC) -E -MM $(CFLAGS) $(CFILES) > .depend

-include .depend


