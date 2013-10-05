#include "Server.h"
#include <iostream>

/**
 * Instantiate the values the server will need to operate.
 */
Server::Server( int port, std::string root ) : port_number( port ), web_root( root ) {
}

/**
 * Begin listening for a connection.
 */
void Server::listen() {
    sock.create();
    sock.bind( port_number );
    sock.listen();

    Socket new_sock;
    sock.accept( new_sock );

    while (true) {
        std::string sock_results;
        new_sock.receive_data( sock_results );

        handle_request( sock_results );

        // Now that the request is handled, let's get another!
        sock.accept( new_sock );
    }
}

/**
 * See a request and respond accordingly.
 */
void Server::handle_request( std::string request_data ) {
    std::cout << request_data;
}