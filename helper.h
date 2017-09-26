#ifndef HELPER_H
#define HELPER_H

bool read_n(int fd, char *buff, int size);
bool write_n(int fd, char *buff, int size);
bool send_message(int fd, int type, char *payload, int size);
int read_message(int fd, char* type, char* buff);
bool send_int(int fd, int type, int payload);

enum message_type{
    // Login
    LOGIN = 1,
    LOGIN_STATUS,

    // invite
    INVITE,
    INVITE_SUCCESS,
    ACCEPT_INVITE,
    INVALID_ACCEPT,
    DECLINE_INVITE,
    NOT_IN_GROUP,
    ALREADY_IN_GROUP,

    // send message
    SEND,
    INVALID_ID,
    CANNOT_SEND,

    // leave group
    LEAVE,
    CANNOT_LEAVE,

    // logout
    LOGOUT
};

#endif
