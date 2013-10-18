#include "Server.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <signal.h> // SIGINT etc.

#define HTTP_OK "200 OK"
#define HTTP_NOT_FOUND "404 NOT FOUND"
#define MAX_CONNECTIONS 256

/**
 * Instantiate the values the server will need to operate.
 */
Server::Server( int port, std::string root ) : port_number( port ), web_root( root ) {
    // ensure the web root doesn't end in a /
    if ( web_root[web_root.length() - 1] == '/' )
        web_root = web_root.substr( 0, web_root.length() - 1 );

    std::cout << "Starting server on port " << port_number << " with root: ";

    // Relative path, print the "cwd/#{doc_root}" on the console
    if ( web_root[0] != '/' ) {
        char buf[PATH_MAX];
        if( getcwd( buf, sizeof(buf) ) )
            std::cout << buf << "/";
    }

    std::cout << web_root << std::endl;
}

/**
 * Kill all child forks on exit
 */
Server::~Server() {
    kill_child_forks( SIGINT );
}

/**
 * Forward any recieved signals to child forks
 */
void Server::kill_child_forks(int sig) {
    for( Server::child_fork_t::iterator iter = child_forks.begin(); iter != child_forks.end(); iter++)
        kill( *iter, sig );

    waitpid( -1, NULL, 0 );
    child_forks.clear();
}

void Server::child_exited(pid_t p) {
    child_fork_t::iterator iterator = child_forks.find(p);

    if( iterator != child_forks.end() )
        child_forks.erase(iterator);
}

/**
 * Begin listening for a connection.
 */
void Server::listen() {
    sock.create();
    sock.bind( port_number );
    sock.listen( MAX_CONNECTIONS );

    while (true) {
        Socket new_sock;
        sock.accept( new_sock );

        // Fork a child to handle the request
        pid_t pid = fork();
        bool is_child = false;

        // fork succeeded
        if( pid > 0 ) {
            child_forks.insert( pid );
            continue;
        } else if( pid == 0 ) {
            is_child = true;
        }

        // If we fail to fork, have the parent serve the request

        // Clear list of child pids from the child process,
        // only the parent should kill the children
        if( is_child )
            child_forks.clear();

        std::string sock_results;
        new_sock.receive_data( sock_results );
        handle_request( new_sock, sock_results );

        // Kill child here
        if( is_child )
            exit(0);
    }
}

/**
 * See a request and respond accordingly.
 *
 * Assemble the whole HTTP response, including the headers and content.
 * This should be sent directly back to the browser.
 */
void Server::handle_request( Socket& response_socket, const std::string& request_data ) {
    std::string file_name = extract_requested_file( request_data );
    std::string file_ext  = "";
    std::string mime_type = "";
    std::string response_code = "";

    std::stringstream header;
    std::ifstream ifs;
    std::string response_data;

    // Grab the file extention if we find a '.'
    size_t ext_index = file_name.rfind( '.' );
    if ( std::string::npos != ext_index )
        file_ext = file_name.substr( ext_index, std::string::npos );

    // Grab mime type
    std::transform(file_ext.begin(), file_ext.end(), file_ext.begin(), ::tolower);
    if ( ".html" == file_ext )
        mime_type = "text/html; charset=utf-8";
    else if ( ".gif" == file_ext )
        mime_type = "image/gif";
    else if( ".jpeg" == file_ext || ".jpg" == file_ext )
        mime_type = "image/jpeg";
    else
        mime_type = "test/plain";

    // Attempt to open the file if valid name
    if ( file_name.size() > 0 ) {
        file_name = web_root + file_name;
        ifs.open( file_name.c_str(), std::ifstream::in );
    }

    if ( ifs.is_open() )
        response_code = HTTP_OK;
    else
        response_code = HTTP_NOT_FOUND;

    char c = ifs.get();
    while ( ifs.good() ) {
        response_data.push_back(c);
        c = ifs.get();
    }

    ifs.close();

    // Build response header
    header << "HTTP/1.1 " << response_code << std::endl;

    if ( HTTP_NOT_FOUND == response_code ) {
        header << "Content-Length: " << response_code.size() << "\n\n";
        response_socket.send_data( header.str() );
        response_socket.send_data( response_code );
        std::cout << "*** Client Request ***\n\n" << request_data << std::endl;
        return;
    }

    header << "Content-Type: " << mime_type << std::endl;
    header << "Content-Length: " << response_data.size() << "\n\n";

    response_socket.send_data( header.str() );
    response_socket.send_data( response_data );

    std::cout << "*** Client Request ***\n\n" << request_data << std::endl;
}

/**
 * We need to figure out what file we're after. Here's an easy way.
 * When we parse the HTTP header, we only care about the first line.
 */
std::string Server::extract_requested_file( std::string request_data ) {
    char* request_copy = new char[request_data.size() + 1];
    memset( request_copy, '\0', request_data.size() + 1 );
    strncpy( request_copy, request_data.c_str(), request_data.size() );

    // Token now holds the method like GET or POST
    char *token = strtok( request_copy, " " );

    // Now we have the url
    token = strtok( NULL, " " );

    std::string file_name = "";

    if ( NULL != token )
        file_name = token;

    delete request_copy;

    // Bind the url root to index.html or remove the slash before the path
    if ( "/" == file_name )
        file_name = "index.html";
    else if( file_name[0] == '/' )
        file_name = file_name.substr(1, std::string::npos);

    return file_name;
}
