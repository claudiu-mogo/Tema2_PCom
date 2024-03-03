#include "helper.h"

/* function in which we receive a message */
short my_recv(int sock, char *mess)
{
    short len_to_rec, total_ret = 0, ret;
    /* firstly, we receive the length of the actual message */
    recv(sock, &len_to_rec, sizeof(len_to_rec), 0);

    /* loop until we receive the whole message */
    while (len_to_rec > total_ret) {
        ret = recv(sock, mess + total_ret, len_to_rec - total_ret, 0);
        total_ret += ret;
    }

    return total_ret;
}

/* function to send a message of length len */
int my_send(int sock, char *mess, short len)
{
    int ret;
    short lng = len;
    
    /* send message length */
    ret = send(sock, (char *)&lng, sizeof(lng), 0);
    if (ret < 0)
        return ret;

    /* send the message itself */
    ret = send(sock, mess, len, 0);
    if (ret < 0)
        return ret;
    
    return 1;
}