#include "helper.h"

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sock_tcp;
    int flag = 1;
    struct sockaddr_in serv_addr, received;
    memset(&serv_addr, 0, sizeof(serv_addr));
    char *stdin_buffer = calloc(200, sizeof(char));
    char *message_received = calloc(2000, sizeof(char));

    /* set a socket for tcp connection with the server */
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_tcp < 0) {
        perror("[SUBSCRIBER] failed socket\n");
        exit(0);
    }
    setsockopt(sock_tcp, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

    /* connect to the given server */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    serv_addr.sin_addr.s_addr = inet_addr(argv[2]);

    if(connect(sock_tcp, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("[SUBSCRIBER] Unable to connect to the server\n");
        exit(0);
    }
    memcpy(stdin_buffer, argv[1], strlen(argv[1]));

    /* send a message with the name of the user */
    if(my_send(sock_tcp, stdin_buffer, strlen(stdin_buffer)) < 0){
        perror("[SUBSCRIBER] Unable to send initial message\n");
        exit(0);
    }
    
    /* add I/O multiplex */
    struct pollfd *pfds = malloc(50 * sizeof(struct pollfd));
    int nfds = 0, ret;

    /* add stdin as listener socket */
    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds++].events = POLLIN;

    pfds[nfds].fd=sock_tcp;
    pfds[nfds++].events = POLLIN;

    while(1) {
        poll(pfds, nfds, -1);

        if ((pfds[0].revents & POLLIN) != 0) {
            /* parse data from stdin */
            memset(stdin_buffer, 0, 50);
            if ((ret = read(pfds[0].fd, stdin_buffer, 50)) < 0) {
                perror("[SUBSCRIBER] couldn't read from stdin\n");
                exit(0);
            }
            if(ret == 0) {
                perror("[SUBSCRIBER] stdin closed connection\n");
                exit(0);
            }
            if (!strcmp(stdin_buffer, "exit\n")) {
                /* if the message was exit -> notify the server and quit */
                if(send(sock_tcp, stdin_buffer, strlen(stdin_buffer), 0) < 0){
                    perror("[SUBSCRIBER] Unable to send exit message\n");
                    exit(0);
                }
                memset(stdin_buffer, 0, 200);
                close(sock_tcp);
                break;
            } else {
                /* otherwise, send the message as it is to the server */
                if(send(sock_tcp, stdin_buffer, strlen(stdin_buffer), 0) < 0){
                    perror("[SUBSCRIBER] Unable to send message\n");
                    exit(0);
                }
                memset(stdin_buffer, 0, 200);
            }
        } else if ((pfds[1].revents & POLLIN) != 0) {
            /* parse data received from the server */
            ret = my_recv(pfds[1].fd, message_received);
            if (ret < 0) {
                perror("la receive\n");
                puts(message_received);
                return -1;
            }

            /* ack for subscribe confirmation */
            if (strstr(message_received, "Subscribed ") == message_received)
                printf("%s\n", message_received);

            /* if what we received is longer than 50 characters, it is a udp message */
            else if (ret > 50) {
                memcpy(&received, message_received, sizeof(struct sockaddr_in));

                /* print udp data */
                printf("%s:%d", inet_ntoa(received.sin_addr), ntohs(received.sin_port));
                int off = sizeof(struct sockaddr_in);

                /* print topic name */
                char save = message_received[50 + off];
                message_received[50 + off] = '\0';
                printf(" - %s - ", message_received + off);
                message_received[50 + off] = save;

                /* parse the content, depending on the data type */
                switch (message_received[50 + off]) {
                    case 1:
                        printf("SHORT_REAL - %.2f\n", (ntohs(*(short *)(message_received + 51 + off))) / 100.0);
                        break;
                    case 2:
                        printf("FLOAT - %s%.*f\n", (message_received[51 + off] == 1 ? "-" : ""),
                                        message_received[56 + off],
                                        (ntohl(*(int *)(message_received + 52 + off))) 
                                            / pow(10, message_received[56 + off]));
                        break;
                    case 3:
                        printf("STRING - ");
                        puts(message_received + 51 + off);
                        break;
                    case 0:
                        printf("INT - %s%d\n", (message_received[51 + off] == 1 ? "-" : "")
                                , (ntohl(*(int *)(message_received + 52 + off))));
                        break;
                    default:
                        printf("nu\n");
                        break;
                }
            } else {

                /* get ack after trying to disconnect and actually disconnect */
                if (strstr(message_received, "Client ") == message_received) {
                    close(sock_tcp);
                    memset(message_received, 0, 2000);
                    break;
                } else {

                    /* the server wants to close, the client needs to close too */
                    if (strstr(message_received, "Exit ") == message_received) {
                        close(sock_tcp);
                        memset(message_received, 0, 2000);
                        break;
                    }
                    printf("%s\n", message_received);
                }
            }
            memset(message_received, 0, 2000);
        }
    }

    return 0;
}