#include <iostream>
#include "RDTConnection.h"

#define DEFAULT_PORT 9529

RDTConnection *conn = NULL;

void sig_handler( int signal ) {
    printf( "Caught signal %i, exiting\n", signal );

    if(conn) {
        delete conn;
        conn = NULL;
    }

    exit(signal);
}

int main() {
    conn = new RDTConnection();
    if (!conn->connect("127.0.0.1", DEFAULT_PORT)) {
        std::cout << "Connection failed, aborting" << std::endl;
        exit(-1);
    }

    std::string server_msg;
    conn->send_data("Hello world! This is the client!");
    conn->receive_data(server_msg);
    conn->close();

    std::cout << "Server said: \"" << server_msg << "\"" << std::endl;
    return 0;
}
