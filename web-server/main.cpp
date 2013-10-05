/**
 * Simple Web Server
 * UCLA, Computer Science 118. Fall 2013.
 *
 * @author Phil Crumm pcrumm@ucla.edu
 * @author Ivan Petkov ipetkov@ucla.edu
 * @license gpl2
 *
 * This is the "entry point" for our application. It makes sure everything is
 * up and running and hands off the appropriate data for response.
 */

#define DEFAULT_PORT 9529
#include <unistd.h> // getopt
#include <iostream> // std::cout, etc.
#include "Sock.h"

/**
 * usage()
 *
 * Display the usage message and end execution.
 */
void usage() {
    std::cout << "Simple Webserver for CS118, Fall 2013\n";
    std::cout << "Created by Phil Crumm and Ivan Petkov.\n";
    std::cout << "\nUsage:\n" ;
    std::cout << "./server - Run the server on the default port, " << DEFAULT_PORT << "\n";
    std::cout << "./server -p X - Run the server on specified port X\n";
    std::cout << "./server -r X - Specify the document root as X. Must be an absolute path and end in a slash. Defaults to /var/www\n";
    std::cout << "./server -h - Display this message.\n";

    exit( EXIT_SUCCESS );
}

int main( int argc, char **argv ) {
    int port_number = 0;
    std::string doc_root = "";

    for( ;; )
        switch( getopt( argc, argv, "p:hr:" ) ) {
            case 'p': port_number = atoi( optarg ); break;
            case 'h': default: usage(); break;
            case 'r': doc_root.assign( optarg );
            case -1: goto options_exhausted;
        }
    options_exhausted:;

    port_number = port_number != 0 ? port_number : DEFAULT_PORT;
    doc_root = doc_root == "" ? "/var/www/" : doc_root;

    std::cout << "Starting Simple Web Server on port " << port_number << "\n";
    std::cout << "Serving documents from " << doc_root << "\n";

    // Let's create our socket...
    Socket sock;
    sock.create();
    sock.bind( port_number );
    sock.listen();
    Socket new_sock;
    sock.accept( new_sock );

    while (true) {
        std::string sock_results;
        new_sock.receive_data( sock_results );
        std::cout << sock_results;
        sock.accept( new_sock );
    }


    return EXIT_SUCCESS;
}