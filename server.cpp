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
std::condition_variable cv;

void do_login(int client_fd);
void after_login(int client_fd, int who);
void sender();

int fds[4] = {-1,-1,-1,-1};

struct query{
    // just pair of (type, message)
    char type;
    std::vector<char> message;
    query(){}
    query(char _type, char* buff, int size):
        type(_type), message(buff, buff+size){}
    query(int _type, char* buff, int size):
        type(_type), message(buff, buff+size){}
};
std::queue<query> client_queue[4]; // unsended messages
bool in_group[4] = {true, true, false, false}; // user i is in group?
bool invited[4]; // user i has been invited?

void sender(){
    query q;
    int fd;
    char buff[MSGSIZE];
    while(1){
        bool to_send = false;
        int target = -1;
        for(int i=0;i<4;i++){
            // find unsended && can send query
            std::unique_lock<std::mutex> lck(mtx);
            if(fds[i] > 0 && !client_queue[i].empty()){
                fd = fds[i];
                q = client_queue[i].front();
                to_send = true;
                target = i;
            }
            lck.unlock();
            if(to_send) break;
        }

        if(to_send){
            // found!
            std::copy(q.message.begin(), q.message.end(), buff);
            if(send_message(fd, q.type, buff, q.message.size())){
                // if success, pop from queue.
                std::unique_lock<std::mutex> lck(mtx);
                client_queue[target].pop();
                lck.unlock();
            }
        }
        else{
            // not found! sleep.
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait(lck);
            lck.unlock();
        }
    }
}

void do_login(int client_fd){
    char buff[MSGSIZE];
    int msgsize;
    char type;
    while((msgsize = read_message(client_fd, &type, buff)) >= 0){
        if(type==LOGIN && msgsize==1){
            // msgsize==1: just for checking ID.
            char id = buff[0];
            if('A' <= id && id <= 'D'){
                // valid ID
                int who = id - 'A';
                after_login(client_fd, who);

                // reach here means 'connection closed'
                std::unique_lock<std::mutex> lck(mtx);
                if(fds[who] == client_fd) // duplicated login handling.
                    fds[who] = -1;
                lck.unlock();
                break;
            }
            else{
                fprintf(stderr, "%d Login Failed.\n", client_fd);
                if(!send_int(client_fd, LOGIN_STATUS, -1)) break;
            }
        }
        else{
            fprintf(stderr, "%d Login Failed.\n", client_fd);
            if(!send_int(client_fd, LOGIN_STATUS, -1)) break;
        }
    }

    fprintf(stderr, "%d connection closed.\n", client_fd);
    close(client_fd);
}

void add_queue(int target, const query& q){
    // if we add messages to queue, wake the sender thread up.
    std::unique_lock<std::mutex> lck(mtx);
    client_queue[target].push(q);
    cv.notify_one(); // wake up!
    lck.unlock();
}

void after_login(int client_fd, int who){
    fprintf(stderr, "User %d login. Client_fd: %d\n", who, client_fd);
    int unread;

    // now we update fds[who], so sender thread can send messages to 'who'.
    // before send messages, we should count unread messages.
    std::unique_lock<std::mutex> lck(mtx);
    unread = client_queue[who].size();
    if(fds[who] != -1) close(fds[who]);
    fds[who] = client_fd;
    lck.unlock();

    // login succeeded!
    if(!send_int(client_fd, LOGIN_STATUS, unread)) return;

    // from now, all messages is sended async.
    lck.lock();
    cv.notify_one(); // wake up!
    lck.unlock();

    char type;
    char buff[MSGSIZE];
    int msgsize;
    while((msgsize = read_message(client_fd, &type, buff)) >= 0){
        if(type == INVITE){
            // invite.
            if(msgsize != 1 || !('A' <= buff[0] && buff[0] <= 'D')){ // id check.
                add_queue(who, query(INVALID_ID, nullptr, 0));
                continue;
            }

            int invite = buff[0] - 'A';
            bool me, you;

            lck.lock();
            me = in_group[who];
            you = in_group[invite];
            lck.unlock();

            // 'me' should be true, 'you' should be false.
            if(!me) add_queue(who, query(NOT_IN_GROUP, nullptr, 0));
            else if(you){
                *(int*)buff = invite;
                add_queue(who, query(ALREADY_IN_GROUP, buff, 4));
            }
            else{
                lck.lock();
                invited[invite] = true;
                lck.unlock();

                *(int*)buff = who;

                // send invite message and response of invite query.
                add_queue(invite, query(INVITE, buff, 4));
                add_queue(who, query(INVITE_SUCCESS, nullptr, 0));
            }
        }
        else if(type == ACCEPT_INVITE){
            bool can_join;
            lck.lock();
            can_join = invited[who]; // has been invited?
            lck.unlock();

            if(can_join){
                // if then, join the group.
                lck.lock();
                in_group[who] = true;
                lck.unlock();
                add_queue(who, query(ACCEPT_INVITE, nullptr, 0));
            }
            else{
                add_queue(who, query(INVALID_ACCEPT, nullptr, 0));
            }
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

            // can send message iff user is in group.
            if(!cur_group[who])
                add_queue(who, query(CANNOT_SEND, nullptr, 0));
            else{
                // found other user in group.
                for(int i=0;i<4;i++) if(i!=who && cur_group[i]){
                    msgsize += 4; // uid = ID('A'~'D') - 'A'
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
            if(!can_leave) // iff user is in group.
                add_queue(who, query(CANNOT_LEAVE, nullptr, 0));
            else{
                // leaving group.
                lck.lock();
                in_group[who] = false;
                invited[who] = false;
                lck.unlock();
                add_queue(who, query(LEAVE, nullptr, 0));
            }
        }
        else if(type == LOGOUT) return;
    }

    fprintf(stderr, "User %d, fd %d connection closing.\n", who, client_fd);
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
    std::thread(sender).detach(); // start sender thread before opening the port.

    while(1){
        sock_fd_client = accept(sock_fd_server,
                (struct sockaddr *) &client_addr,
                (socklen_t *) &client_addr_size);
        if(sock_fd_client > 0){
            // accepted! start user thread.
            std::thread(do_login, sock_fd_client).detach();
        }
    }

    close(sock_fd_server);
    return 0;
}

