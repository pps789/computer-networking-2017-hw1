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
#include "helper.h"

const int PORT = 20400;
const int MSGSIZE = 256;

void do_login(int fd);
void after_login(int fd);

void do_login(int fd){
    char buff[MSGSIZE];
    while(1){
        fgets(buff, sizeof(buff), stdin);
        send_message(fd, LOGIN, buff, strlen(buff) - 1);
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

void after_login(int fd){
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

