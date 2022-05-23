CC			=  gcc
AR			=  ar
CFLAGS		+= -std=c99 -Wall -g -D_POSIX_C_SOURCE=200112L
ARFLAGS		=  rvs
INCLUDES	= -I. -I ./includes
LDFLAGS 	= -L.
OPTFLAGS	= -O0
LIBS		= -lpthread
SUBDIRS		= bin
LOGSDIR		= logs
TEST1FILES	= test/test1/store_files
TEST2FILES1	= test/test2/victims1
TEST2FILES2	= test/test2/victims2
TEST3FILES	= test/test3/store_files

.PHONY:	clean cleanall

all	: server	client

list.o	: includes/list.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/list.c $(LDFLAGS) $(LIBS)
	@mv list.o bin/list.o

icl_hash.o	: includes/icl_hash.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/icl_hash.c $(LDFLAGS) $(LIBS)
	@mv icl_hash.o bin/icl_hash.o

threadpool.o	:	includes/threadpool.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/threadpool.c $(LDFLAGS) $(LIBS)
	@mv threadpool.o bin/threadpool.o

read_write_lock.o	: includes/read_write_lock.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/read_write_lock.c $(LDFLAGS) $(LIBS)
	@mv read_write_lock.o bin/read_write_lock.o

request_handler.o	: includes/request_handler.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/request_handler.c $(LDFLAGS) $(LIBS)
	@mv request_handler.o bin/request_handler.o

storage.o	: includes/storage.h includes/list.h includes/icl_hash.h includes/read_write_lock.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/storage.c $(LDFLAGS) $(LIBS)
	@mv storage.o bin/storage.o

API.o	: includes/list.h	includes/util.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/API.c $(LDFLAGS) $(LIBS)
	@mv API.o bin/API.o

client.o :
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/client.c $(LDFLAGS) $(LIBS)
	@mv client.o bin/client.o

server.o : includes/icl_hash.h includes/list.h  includes/storage.h includes/threadpool.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/server.c $(LDFLAGS) $(LIBS)
	@mv server.o bin/server.o

server	:	server.o	icl_hash.o	storage.o	list.o threadpool.o	request_handler.o read_write_lock.o
	gcc 	bin/request_handler.o	bin/threadpool.o bin/list.o bin/storage.o  bin/icl_hash.o bin/server.o bin/read_write_lock.o -o server -lpthread

client	:	client.o	API.o	list.o
	gcc bin/list.o	bin/API.o	bin/client.o -o client -lpthread

clean cleanall:
	@echo "Pulizia file in corso"
	rm -f server client *.o *~ *.a *.sk
	cd $(SUBDIRS) && rm -f *.o *~ *.a
	cd $(LOGSDIR) && rm -f *.txt
	# Pulizia test
	cd $(TEST1FILES) && rm -f *
	cd $(TEST2FILES1) && rm -f *
	cd $(TEST2FILES2) && rm -f *
	cd $(TEST3FILES) && rm -f *

test1: client server
	@chmod +x scripts/script1.sh
	scripts/script1.sh

test2: client server
	@chmod +x scripts/script2.sh
	scripts/script2.sh

test3: client server
	@chmod +x scripts/script3.sh
	@chmod +x scripts/startClient.sh
	scripts/script3.sh

