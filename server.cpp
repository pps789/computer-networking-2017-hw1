#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <queue>

const int PORT = 20400;
const int MSGSIZE = 256;
char ids[][2] = {"A", "B", "C", "D"};
bool is_login[4];

enum message_type{
    LOGIN = 1, INVITE, SEND, ACCEPT_INVITE, DECLINE_INVITE
};

bool read_n(int fd, char *buff, int size){
    while(size){
        int rd = read(fd, buff, size);
        if(rd < 0) return false;
        size -= rd;
        buff += rd;
    }
    return true;
}

void do_login(int client_fd);
void after_login(int client_fd, int who);

/*
struct query;
std::queue<query> client_queue[4];
*/

void do_login(int client_fd){
    char buff[MSGSIZE];
    int msgsize;
    while(read_n(client_fd, (char*)msgsize, 4)){
        char type;
        if(!read_n(client_fd, &type, 1)) return;
        msgsize--;
        if(!read_n(client_fd, buff, msgsize)) return;

        if(type==1 && msgsize==1){
            char id = buff[0];
            if('A' <= id && id <= 'D'){
                after_login(client_fd, id - 'A');
            }
        }
    }
}

void after_login(int client_fd, int who){
    printf("Hello, user %d!\n", who);
    close(client_fd);
};

int main(){
    int sock_fd_server = 0;
    int sock_fd_client = 0;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    int client_addr_size;

    sock_fd_server = socket(PF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    bind(sock_fd_server, (struct sockaddr*) &server_addr,
            sizeof(struct sockaddr_in));
    listen(sock_fd_server, 5);

    client_addr_size = sizeof(struct sockaddr_in);

    while(1){
        sock_fd_client = accept(sock_fd_server,
            (struct sockaddr *) &client_addr,
            (socklen_t *) &client_addr_size);
        if(sock_fd_client > 0){
            std::thread(do_login, sock_fd_client).detach();
        }
    }

    close(sock_fd_server);
    return 0;
}

