#include <iostream>
#include <cstdlib>
#include <cstring> // memset, etc.
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "RDTConnection.h"

#define DEFAULT_PORT 9529
#define WINDOW_SIZE 1024
#define MAX_BUF_SIZE 8192

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

    int port = DEFAULT_PORT;
    int cwnd = WINDOW_SIZE;
    double pdrop = 0;
    double pcorrupt = 0;

    switch (std::min(argc, 4)) {
        case 4:
            pcorrupt = atof(argv[--argc]) * 100;
        case 3:
            pdrop = atof(argv[--argc]) * 100;
        case 2:
            cwnd = atoi(argv[--argc]);
        case 1:
            port = atoi(argv[--argc]);
        case 0: // program name
        default:
            break;
    }

    server = new RDTConnection(cwnd, pdrop, pcorrupt);

    if (!server->listen(port)) {
        std::cout << "server listen failed, aborting" << std::endl;
        delete server;
        server = NULL;
        exit(-1);
    } else {
        std::cout << "Listening on port " << server->port_number() << std::endl;
    }

    std::string remote_msg;
    char buf[MAX_BUF_SIZE+1];
    memset(buf, 0, MAX_BUF_SIZE+1);

    while (true) {
        if (!server->accept())
            continue;

        server->receive_data(remote_msg);

        int fd = open(remote_msg.c_str(), O_RDONLY);
        if (fd == -1) {
            std::cout << "Invalid file \"" << remote_msg << "\" requested" << std::endl;
            server->close();
            continue;    
        }

        int read_len = 0;
        std::string file_contents = "";
        while( (read_len = read(fd, buf, MAX_BUF_SIZE)) > 0) {
            file_contents.append(buf, read_len);
            memset(buf, 0, MAX_BUF_SIZE);
        }

        server->send_data(file_contents);
        server->close();
    }

    return 0;
}
