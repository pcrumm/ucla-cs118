#ifndef serv_head
#define serv_head

#include <string>
#include "Sock.h"

class Server {
public:
    Server( int port, std::string root );

    void listen();
    std::string handle_request( std::string request_data );

    std::string extract_requested_file( std::string request_data );

private:
    int port_number;
    std::string web_root;
    Socket sock;
};

#endif