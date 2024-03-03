#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

#define _GNU_SOURCE
#include <poll.h>
#include <sys/poll.h>
#include <netinet/tcp.h>

short my_recv(int sock, char *mess);
int my_send(int sock, char *mess, short len);