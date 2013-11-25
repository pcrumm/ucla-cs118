#include <iostream>
#include <cstdlib>
#include <signal.h>
#include "../RDTConnection.h"

#define DEFAULT_PORT 9529
#define WINDOW_SIZE 1024

RDTConnection *server = NULL;

void sig_handler( int signal ) {
    std::cout << "Caught signal " << signal << ", exiting" << std::endl;

    if(server) {
        delete server;
        server = NULL;
    }

    exit(signal);
}

int main( int argc, char **argv ) {
    signal( SIGHUP, sig_handler );
    signal( SIGINT, sig_handler );
    signal( SIGTERM, sig_handler );

    server = new RDTConnection(WINDOW_SIZE);

    if (!server->listen(DEFAULT_PORT)) {
        std::cout << "server listen failed, aborting" << std::endl;
        exit(-1);
    } else {
        std::cout << "Listening on port " << server->port_number() << std::endl;
    }

    std::string remote_msg;
    while (true) {
        if (!server->accept())
            continue;

        server->receive_data(remote_msg);
        std::string server_reply = "Hello from the server!";

        // good chars are between 32 and 126
        int num_garbage_chars = 2000;
        char test_data[num_garbage_chars];
        for (int j = 0; j < 1024; j++) {
            for (int i = 0; i < num_garbage_chars - 1; i++) {
                test_data[i] = (i % (126 - 32)) + 32;
            }
            server_reply.append(test_data);
        }

        std::cout << "Sending data with length " << server_reply.length() << std::endl;

        server->send_data(server_reply);
        std::cout << "Remote host said: \"" << remote_msg << "\"" << std::endl;
        server->close();
    }

    return 0;
}
