CC		=  gcc
AR              =  ar
CFLAGS	        += -std=c99 -Wall -Werror -g
ARFLAGS         =  rvs
INCLUDES	= -I. -I ./utils/includes
LDFLAGS 	= -L.
OPTFLAGS	= -O3 
LIBS            = -lpthread

server.c : util.h  icl_hash.h  conn.h
  $(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean : 
  rm -f *.o *~ *.a
