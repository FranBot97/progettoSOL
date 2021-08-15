CC		=  gcc
AR              =  ar
CFLAGS	        += -std=c99 -Wall -g
ARFLAGS         =  rvs
INCLUDES	= -I. -I ./utils/includes
LDFLAGS 	= -L.
OPTFLAGS	= -O3 
LIBS            = -lpthread
SUBDIRS = bin

all	: client server

server	: server.o
	gcc bin/server.o -o server

client	:	client.o	api.o
	gcc bin/api.o bin/client.o -o client


server.o : utils/includes/util.h  utils/includes/icl_hash.h  utils/includes/conn.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/server.c $(LDFLAGS) $(LIBS)
	@mv server.o bin/server.o

client.o	:	utils/includes/util.h  utils/includes/icl_hash.h  utils/includes/conn.h utils/includes/api.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/client.c $(LDFLAGS) $(LIBS)
	@mv client.o bin/client.o

icl_hash.o	:
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/icl_hash.c $(LDFLAGS) $(LIBS)
	@mv icl_hash.o bin/icl_hash.o

api.o	:	src/api.c utils/includes/api.h utils/includes/conn.h utils/includes/util.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c src/api.c $(LDFLAGS) $(LIBS)
	@mv api.o bin/api.o




clean :
	rm -f *.o *~ *.a
	cd $(SUBDIRS) && rm -f *.o *~ *.a
