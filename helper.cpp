#include "helper.h"
#include <unistd.h>
#include <cstring>

const int MSGSIZE = 256;

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

bool send_int(int fd, int type, int payload){
    char buff[9];
    (*(int*)buff) = 4;
    buff[4] = type;
    (*(int*)(buff+5)) = payload;

    return write_n(fd, buff, 9);
}

int read_message(int fd, char* type, char* buff){
    int msgsize;
    if(!read_n(fd, (char*)&msgsize, 4)) return -1;
    if(!read_n(fd, type, 1)) return -1;
    if(!read_n(fd, buff, msgsize)) return -1;
    return msgsize;
}
