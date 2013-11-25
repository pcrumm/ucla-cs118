#include <iostream>
#include <cstdlib>
#include <signal.h>
#include "../RDTConnection.h"

#define DEFAULT_PORT 9529
#define WINDOW_SIZE 1024

RDTConnection *conn = NULL;

void sig_handler( int signal ) {
    std::cout << "Caught signal " << signal << ", exiting" << std::endl;

    if(conn) {
        delete conn;
        conn = NULL;
    }

    exit(signal);
}

int main() {
    conn = new RDTConnection(WINDOW_SIZE);
    if (!conn->connect("127.0.0.1", DEFAULT_PORT)) {
        std::cout << "Connection failed, aborting" << std::endl;
        exit(-1);
    }

    std::string server_msg;
    conn->send_data("Hello world! This is the client!");
    conn->receive_data(server_msg);
    conn->close();

    std::cout << "Server sent response with length \"" << server_msg.length() << "\"" << std::endl;
    return 0;
}
