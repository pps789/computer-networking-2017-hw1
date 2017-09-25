#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <queue>
#include <vector>
#include <algorithm>
#include "helper.h"

const int PORT = 20400;
const int MSGSIZE = 256;
std::mutex mtx;

void do_login(int client_fd);
void after_login(int client_fd, int who);
void sender();

int fds[4] = {-1,-1,-1,-1};

struct query{
    char type;
    std::vector<char> message;
    query(){}
    query(char _type, char* buff, int size):
        type(_type), message(buff, buff+size){}
    query(int _type, char* buff, int size):
        type(_type), message(buff, buff+size){}
};
std::queue<query> client_queue[4];
bool in_group[4] = {true, true, false, false};
bool invited[4];

void sender(){
    timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000;
    query q;
    int fd;
    char buff[MSGSIZE];
    while(1){
        for(int i=0;i<4;i++){
            nanosleep(&ts, nullptr);
            bool to_send = false;
            std::unique_lock<std::mutex> lck(mtx);
            if(fds[i] > 0 && !client_queue[i].empty()){
                fd = fds[i];
                q = client_queue[i].front();
                to_send = true;
            }
            lck.unlock();

            if(to_send){
                to_send = false;
                std::copy(q.message.begin(), q.message.end(), buff);
                bool success = send_message(fd, q.type, buff, q.message.size());
                if(success){
                    lck.lock();
                    client_queue[i].pop();
                    lck.unlock();
                }
            }
        }
    }
}

void do_login(int client_fd){
    char buff[MSGSIZE];
    int msgsize;
    char type;
    while((msgsize = read_message(client_fd, &type, buff)) >= 0){
        if(type==LOGIN && msgsize==1){
            char id = buff[0];
            if('A' <= id && id <= 'D'){
                int who = id - 'A';
                int unread;

                std::unique_lock<std::mutex> lck(mtx);
                unread = client_queue[who].size();
                lck.unlock();

                send_int(client_fd, LOGIN_STATUS, unread);
                after_login(client_fd, who);
            }
            else{
                printf("Login Failed.\n");
                send_int(client_fd, LOGIN_STATUS, -1); // LOGIN fail
            }
        }
        else{
            printf("Login Failed.\n");
            send_int(client_fd, LOGIN_STATUS, -1); // LOGIN fail
        }
    }

    printf("%d connection closed.\n", client_fd);
}

void add_queue(int target, const query& q){
    std::unique_lock<std::mutex> lck(mtx);
    client_queue[target].push(q);
    lck.unlock();
}

void after_login(int client_fd, int who){
    printf("User %d login. Client_fd: %d\n", who, client_fd);

    std::unique_lock<std::mutex> lck(mtx);
    fds[who] = client_fd;
    lck.unlock();

    char type;
    char buff[MSGSIZE];
    int msgsize;
    while((msgsize = read_message(client_fd, &type, buff)) >= 0){
        if(type == INVITE){
            if(msgsize != 1 || !('A' <= buff[0] && buff[0] <= 'D')){
                add_queue(who, query(INVALID_ID, nullptr, 0));
                continue;
            }
            
            int invite = buff[0] - 'A';
            bool me, you;
            lck.lock();
            me = in_group[who];
            you = in_group[invite];
            lck.unlock();

            if(!me) add_queue(who, query(NOT_IN_GROUP, nullptr, 0));
            else if(you){
                *(int*)buff = invite;
                add_queue(who, query(ALREADY_IN_GROUP, buff, 4));
            }
            else{
                lck.lock();
                invited[invite] = true;
                lck.unlock();
                *(int*)buff = invite;
                add_queue(invite, query(INVITE, buff, 4));
            }
        }
        else if(type == ACCEPT_INVITE){
            bool can_join;
            lck.lock();
            can_join = invited[who];
            lck.unlock();
            if(can_join){
                lck.lock();
                in_group[who] = true;
                lck.unlock();
            }
            add_queue(who, query(ACCEPT_INVITE, nullptr, 0));
        }
        else if(type == DECLINE_INVITE){
            lck.lock();
            invited[who] = false;
            lck.unlock();
            add_queue(who, query(DECLINE_INVITE, nullptr, 0));
        }
        else if(type == SEND){
            bool cur_group[4];
            lck.lock();
            for(int i=0;i<4;i++) cur_group[i] = in_group[i];
            lck.unlock();
            if(!cur_group[who])
                add_queue(who, query(CANNOT_SEND, nullptr, 0));
            else{
                for(int i=0;i<4;i++) if(i!=who && cur_group[i]){
                    msgsize += 4;
                    for(int j=msgsize-1;j>=4;j--)
                        buff[j] = buff[j-4];
                    *(int*)buff = who;
                    add_queue(i, query(SEND, buff, msgsize));
                }
            }
        }
        else if(type == LEAVE){
            bool can_leave;
            lck.lock();
            can_leave = in_group[who];
            lck.unlock();
            if(!can_leave)
                add_queue(who, query(CANNOT_LEAVE, nullptr, 0));
            else{
                lck.lock();
                in_group[who] = false;
                invited[who] = false;
                lck.unlock();
                add_queue(who, query(LEAVE, nullptr, 0));
            }
        }
        else if(type == LOGOUT) break;
    }
    
    lck.lock();
    close(fds[who]);
    fds[who] = -1;
    lck.unlock();

    printf("User %d, fd %d connection closing.\n", who, client_fd);
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
    if(bind(sock_fd_server, (struct sockaddr*) &server_addr,
            sizeof(struct sockaddr_in)) < 0)
        exit(1);
    if(listen(sock_fd_server, 5) < 0)
        exit(1);

    client_addr_size = sizeof(struct sockaddr_in);
    std::thread(sender).detach();

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

