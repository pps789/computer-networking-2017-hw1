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

const int PORT = 20400;
const int MSGSIZE = 256;

enum message_type{
    LOGIN = 1, LOGIN_STATUS, INVITE, SEND, ACCEPT_INVITE, DECLINE_INVITE
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

bool write_n(int fd, char *buff, int size){
    while(size){
        int wr = write(fd, buff, size);
        if(wr < 0) return false;
        size -= wr;
        buff += wr;
    }
    return true;
}

bool send_message(int fd, int type, char *payload, int size){
    char buff[MSGSIZE];
    (*(int*)buff) = size;
    buff[4] = type;
    memcpy(buff+5, payload, size);

    int msgsize = size;
    return write_n(fd, buff, msgsize + 5);
}

bool read_message(int fd, char* type, char* buff){
    int msgsize;
    if(!read_n(fd, (char*)&msgsize, 4)) return false;
    if(!read_n(fd, type, 1)) return false;
    if(!read_n(fd, buff, msgsize)) return false;
    return true;
}

void do_login(int fd);
void after_login(int fd);

void do_login(int fd){
    char buff[MSGSIZE];
    while(1){
        fgets(buff, sizeof(buff), stdin);
        send_message(fd, LOGIN, buff, strlen(buff) - 1);
        while(1){
            char type;
            read_message(fd, &type, buff);
            if(type != LOGIN_STATUS) continue;
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

