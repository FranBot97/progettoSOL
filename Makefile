CC			=  gcc
AR			=  ar
CFLAGS		+= -std=c99 -Wall -g -D_POSIX_C_SOURCE=200112L
ARFLAGS		=  rvs
INCLUDES	= -I. -I ./includes
LDFLAGS 	= -L.
OPTFLAGS	= -O0
LIBS		= -lpthread
SUBDIRS		= bin

.PHONY:	clean

all	: server	client

server	:	server.o	icl_hash.o	file.o	storage.o	list.o threadpool.o	request_handler.o
	gcc bin/request_handler.o	bin/threadpool.o bin/list.o bin/storage.o bin/file.o bin/icl_hash.o bin/server.o -o server -lpthread

client	:	client.o	API.o	list.o
	gcc bin/list.o	bin/API.o	bin/client.o -o client -lpthread

client.o :
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/client.c $(LDFLAGS) $(LIBS)
	@mv client.o bin/client.o

server.o : includes/icl_hash.h includes/file.h includes/storage.h includes/threadpool.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/server.c $(LDFLAGS) $(LIBS)
	@mv server.o bin/server.o

API.o	: includes/list.h	includes/util.h includes/constant_values.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/API.c $(LDFLAGS) $(LIBS)
	@mv API.o bin/API.o

file.o	: includes/file.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/file.c $(LDFLAGS) $(LIBS)
	@mv file.o bin/file.o

storage.o	: includes/storage.h includes/file.h includes/list.h includes/icl_hash.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/storage.c $(LDFLAGS) $(LIBS)
	@mv storage.o bin/storage.o

list.o	: includes/list.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/list.c $(LDFLAGS) $(LIBS)
	@mv list.o bin/list.o

icl_hash.o	: includes/icl_hash.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/icl_hash.c $(LDFLAGS) $(LIBS)
	@mv icl_hash.o bin/icl_hash.o

threadpool.o	:	includes/threadpool.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/threadpool.c $(LDFLAGS) $(LIBS)
	@mv threadpool.o bin/threadpool.o

request_handler.o	: includes/request_handler.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/request_handler.c $(LDFLAGS) $(LIBS)
	@mv request_handler.o bin/request_handler.o

clean :
	@echo "Pulizia file in corso"
	rm -f server client *.o *~ *.a mysock
	cd $(SUBDIRS) && rm -f *.o *~ *.a