# UCLA CS118
# Data Transfer Protocol makefile
# @author Phil Crumm pcrumm@ucla.edu
# @author Ivan Petkov ipetkov@ucla.edu

CC = g++
CFLAGS = -g -Wall -Wextra -Werror

all: sender receiver

test: test_client test_server

SENDER_SOURCES = \
	Sender.cpp \
	RDTConnection.cpp
SENDER_OBJECTS = $(subst .cpp,.o,$(SENDER_SOURCES))

sender: $(SENDER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(SENDER_OBJECTS)

RECEIVER_SOURCES = \
	Receiver.cpp \
	RDTConnection.cpp
RECEIVER_OBJECTS = $(subst .cpp,.o,$(RECEIVER_SOURCES))

receiver: $(RECEIVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(RECEIVER_OBJECTS)

TEST_CLIENT_SOURCES = \
	test/Client.cpp \
	RDTConnection.cpp
TEST_CLIENT_OBJECTS = $(subst .cpp,.o,$(TEST_CLIENT_SOURCES))

test_client: $(TEST_CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(TEST_CLIENT_OBJECTS)

TEST_SERVER_SOURCES = \
	test/Server.cpp \
	RDTConnection.cpp
TEST_SERVER_OBJECTS = $(subst .cpp,.o,$(TEST_SERVER_SOURCES))

test_server: $(TEST_SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(TEST_SERVER_OBJECTS)

clean:
	rm -fr *.o *~ *.bak *.tar.gz core *.core *.tmp sender receiver test_client test_server
	rm -fr test/*.o test/*~ test/*.bak test/*.tar.gz test/core test/*.core test/*.tmp
