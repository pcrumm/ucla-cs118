#include <iostream>
#include <cstdlib>
#include <signal.h>
#include "RDTConnection.h"

#define DEFAULT_PORT 9529
#define WINDOW_SIZE 5

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

    server = new RDTConnection();

    if (!server->listen(DEFAULT_PORT, WINDOW_SIZE)) {
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
        server->send_data("Hello from the server!");
        std::cout << "Remote host said: \"" << remote_msg << "\"" << std::endl;
        server->close();
    }

    return 0;
}
