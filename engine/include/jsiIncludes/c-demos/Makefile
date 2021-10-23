JSIDIR=..
ACFLAGS= -g -Wall -I$(JSIDIR) -DJSI__MEMDEBUG=1
CFLAGS= $(ACFLAGS)  -DJSI__READLINE=1 -I../sqlite/src
LDFLAGS=-lz -lm -ldl -lpthread
SHFLAGS=-shared -fpic 
ALLDEPS=../jsi.c ../jsiOne.c Makefile

all: simple dyn.so prime.so dbdemo litedemo listdemo cmds demo1 demo2 treedemo

simple: simple.c $(ALLDEPS)
	$(CC) $(CFLAGS) -DJSI__SQLITE=1 simple.c -o simple -lz -lm -ldl -lpthread -lsqlite3

cmds: cmds.c ../jsi.c
	$(CC) $(CFLAGS) cmds.c -o cmds -lz -lm -ldl -lpthread

listdemo: listdemo.c $(ALLDEPS)
	$(CC) $(CFLAGS) listdemo.c -o listdemo -lz -lm -ldl -lpthread
	
treedemo: treedemo.c $(ALLDEPS)
	$(CC) $(CFLAGS) treedemo.c -o treedemo -lz -lm -ldl -lpthread

dbdemo: dbdemo.c $(ALLDEPS)
	$(CC) $(CFLAGS) -DJSI__SQLITE=1 dbdemo.c -o dbdemo -lz -lm -ldl -lpthread -lsqlite3

dyn.so: dyn.c $(ALLDEPS)
	$(CC) $(ACFLAGS) -I$(JSIDIR) $(SHFLAGS) dyn.c -o dyn.so

prime.so: prime.c $(ALLDEPS)
	$(CC) $(CFLAGS) $(SHFLAGS) prime.c -o prime.so

demo1: demo1.c $(ALLDEPS)
	$(CC) -o demo1 $(CFLAGS) -DJSI_USER_EXTENSION=Jsi_Initdemo1 demo1.c ../main.c ../jsiOne.c $(LDFLAGS)
        
demo2: demo2.c $(ALLDEPS)
	$(CC) -o demo2 $(CFLAGS) -DJSI_USER_EXTENSION=Jsi_Initdemo2 demo2.c ../main.c ../jsiOne.c $(LDFLAGS)

demo2.so: demo2.c $(ALLDEPS)
	$(CC) $(SHFLAGS) -o demo2.so $(CFLAGS) demo2.c ../main.c $(LDFLAGS)


litedemo: litedemo.c $(ALLDEPS)
	$(CC) $(CFLAGS) -DJSI__SQLITE=1 litedemo.c -o litedemo -lm -lpthread -lsqlite3

testmem:
	JSI_INTERP_OPTS='{memDebug:1}' $(MAKE) test

test: all
	./listdemo
	./litedemo
	./simple
	echo 'exit(0);' | ./dbdemo -benchmark
	./dyn.js
	#echo 'demo(1,2);exit(0)' | ./demo1
	echo 'demos.foo(1,2);exit(0)' | ./demo2
	make -C stubs
	make -C stubs test

clean:
	rm -rf *.so *.o simple listdemo litedemo dbdemo cmds demo1 demo2

