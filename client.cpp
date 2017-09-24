#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <queue>
#include <string>
#include "helper.h"

const int PORT = 20400;
const int MSGSIZE = 256;

void do_login(int fd);
void after_login(int fd);

void do_login(int fd){
    char buff[MSGSIZE];
    while(1){
        fgets(buff, sizeof(buff), stdin);
        int len = strlen(buff);
        if(buff[len-1] == '\n') len--;
        send_message(fd, LOGIN, buff, len);
        while(1){
            char type;
            int msgsize = read_message(fd, &type, buff);
            if(type != LOGIN_STATUS) continue;
            if(msgsize != 4) continue;
            int status = *(int*)(buff);
            if(status == -1){
                printf("Login failed. Please check your id and type again.\n");
                break;
            }
            else{
                printf("Login succeeded! You have unread messages: %d\n", status);
                after_login(fd);
            }
        }
    }
}

void receiver(int fd){
    char buff[MSGSIZE];
    char type;
    int msgsize;
    while((msgsize = read_message(fd, &type, buff)) >= 0){
        if(type == INVITE){
            int who = *(int*)buff;
            printf("%c invites you. /accept or /decline.\n", 'A' + who);
        }
        else if(type == SEND){
            int who = *(int*)buff;
            printf("%c: ", 'A' + who);
            for(int i=4;i<msgsize;i++) printf("%c", buff[i]);
            printf("\n");
        }
        else if(type == ACCEPT_INVITE){
            printf("You accepted invitation. Now you can read messages.\n");
        }
        else if(type == DECLINE_INVITE){
            printf("You declined invitation.\n");
        }
        else if(type == NOT_IN_GROUP){
            printf("You are not a member of a group. You cannot invite someone.\n");
        }
        else if(type == ALREADY_IN_GROUP){
            printf("Target is already in group.\n");
        }
        else if(type == INVALID_ID){
            printf("Please type valid id.\n");
        }
        else if(type == CANNOT_SEND){
            printf("You are not a member of a group. You cannot send messages.\n");
        }
        else if(type == LEAVE){
            printf("You left the group.\n");
        }
        else if(type == CANNOT_LEAVE){
            printf("You are not a member of a group. You cannot leave the group.\n");
        }
        else break;
    }
}

void after_login(int fd){
    std::thread(receiver, fd).detach();
    char buff[MSGSIZE];
    int msgsize;
    while(1){
        fgets(buff, sizeof(buff), stdin);
        msgsize = strlen(buff) - 1;
        if(buff[msgsize-1] == '\n') msgsize--;
        if(buff[0] != '/'){
            send_message(fd, SEND, buff, msgsize);
        }
        else{
            std::vector<char> tk;
            int i = 1;
            for(;i<msgsize;i++){
                if(isspace(buff[i])) break;
                tk.push_back(buff[i]);
            }

            std::string token(tk.begin(), tk.end());
            if(token == "accept"){
                send_message(fd, ACCEPT_INVITE, nullptr, 0);
            }
            else if(token == "decline"){
                send_message(fd, DECLINE_INVITE, nullptr, 0);
            }
            else if(token == "leave"){
                send_message(fd, LEAVE, nullptr, 0);
            }
            else if(token == "logout"){
                send_message(fd, LOGOUT, nullptr, 0);
                break;
            }
            else if(token == "invite"){
                for(;i<msgsize;i++) if(!isspace(buff[i])) break;
                if(i >= msgsize){
                    printf("Please type target ID.\n");
                }
                else{
                    std::vector<char> tg;
                    for(;i<msgsize;i++){
                        if(isspace(buff[i])) break;
                        tg.push_back(buff[i]);
                    }
                    std::copy(tg.begin(), tg.end(), buff);
                    send_message(fd, INVITE, buff, tg.size());
                }
            }
            else{
                printf("Unknown command.\n");
            }
        }
    }
    close(fd);
    exit(0);
};

int main(){
	int sock_fd = 0;
	struct sockaddr_in server_addr;

	sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(PORT);

	if(connect(sock_fd, (struct sockaddr*) &server_addr,
		sizeof(server_addr)) == 0){
        do_login(sock_fd);
    }
    else{
        printf("Connection failed.\n");
        return 0;
    }

	close(sock_fd);
	return 0;
}

