#ifndef serv_head
#define serv_head

#include <string>
#include <set>
#include "Sock.h"

class Server {
public:
    typedef std::set<pid_t> child_fork_t;

    Server( int port, std::string root );
    ~Server();

    void kill_child_forks(int sig);
    void child_exited(pid_t p);
    void listen();
    void handle_request( Socket& response_socket, const std::string& request_data );
    std::string extract_requested_file( std::string request_data );

private:
    int port_number;
    std::string web_root;
    child_fork_t child_forks;
    Socket sock;
};

#endif
