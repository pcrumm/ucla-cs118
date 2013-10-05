#include "Server.h"
#include <iostream>

/**
 * Instantiate the values the server will need to operate.
 */
Server::Server( int port, std::string root ) : port_number( port ), web_root( root ) {
    std::cout << "Starting server on port " << port_number << " with root: " << web_root << std::endl;
}

/**
 * Begin listening for a connection.
 */
void Server::listen() {
    sock.create();
    sock.bind( port_number );
    sock.listen();

    while (true) {
        Socket new_sock;
        sock.accept( new_sock );

        std::string sock_results;
        new_sock.receive_data( sock_results );
        new_sock.send_data( handle_request( sock_results ) );
    }
}

/**
 * See a request and respond accordingly.
 */
std::string Server::handle_request( std::string request_data ) {
    std::cout << request_data;

    return "Message received.";
}