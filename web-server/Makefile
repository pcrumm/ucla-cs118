# UCLA CS118
# Webserver makefile
# @author Phil Crumm pcrumm@ucla.edu
# @author Ivan Petkov ipetkov@ucla.edu

CC = g++
CFLAGS = -g -Wall -Wextra -Werror

all: server

SERVER_SOURCES = \
	main.cpp \
	Sock.cpp
SERVER_OBJECTS = $(subst .cpp,.o,$(SERVER_SOURCES))

server: $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJECTS)

clean:
	rm -fr *.o *~ *.bak *.tar.gz core *.core *.tmp server