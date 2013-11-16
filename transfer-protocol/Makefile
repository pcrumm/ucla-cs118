# UCLA CS118
# Data Transfer Protocol makefile
# @author Phil Crumm pcrumm@ucla.edu
# @author Ivan Petkov ipetkov@ucla.edu

CC = g++
CFLAGS = -g -Wall -Wextra -Werror

all: server client

SERVER_SOURCES = \
	Server.cpp
SERVER_OBJECTS = $(subst .cpp,.o,$(SERVER_SOURCES))

server: $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJECTS)

CLIENT_SOURCES = \
	Client.cpp
CLIENT_OBJECTS = $(subst .cpp,.o,$(CLIENT_SOURCES))

client: $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJECTS)

clean:
	rm -fr *.o *~ *.bak *.tar.gz core *.core *.tmp server client