#include "helper.h"

/* structure for incapsulating the message until sending */
typedef struct message {
    int identifier;
    int len;
    char *message_value;
    int no_possible_clients;
    int no_sent_clients;
    struct message *next;
} Message;

/* structure which contains data for a topic
 * name = topic id
 * mess_head -> pointer to the first message remaining for this topic
 * mess_tail -> pointer to the last message in the topic (to be easier to add)
 * no_messages_ever -> number of all the messages that existed
 */
typedef struct udp_topic {
    char *name;
    Message *mess_head;
    Message *mess_tail;
    int no_messages_ever;
    struct udp_topic *next;
} Topic;

/* structure that is part of a list for a single client
 * contains a pointer to a topic and all the data that user has for it
 */
typedef struct client_topic {
    Topic *topic;
    int sf;
    int last_message_identifier;
    struct client_topic *next;
} Client_topic;

/* structure that defines a client, also part of a list
 * id -> the id of the client
 * connected -> real time status of the client
 */
typedef struct client_info {
    char *id;
    int connected;
    int client_socket;
    Client_topic *topic_element;
    struct client_info *next;
} Tcp_client;

/* function that checks if a user is still subscribed to a topic
 * returns a pointer to that user's data for the topic
 * or NULL if the user doesn't have the topic
 */
Client_topic *user_has_topic(char *name, Client_topic **topics_list)
{
    Client_topic *head = *topics_list;

    while (head != NULL) {
        if (!strcmp(name, head->topic->name)) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

/* function that adds a topic to the user's list after subscribing */
void add_topic_for_user(Client_topic **topic_list, int sf, Topic *topic_to_add)
{
    Client_topic **head = topic_list;
    while ((*head) != NULL)
        head = &(*head)->next;
    (*head) = malloc(sizeof(Client_topic));
    (*head)->last_message_identifier = topic_to_add->no_messages_ever;
    (*head)->sf = sf;
    (*head)->topic = topic_to_add;
    (*head)->next = NULL;
}

/* function that removes a topic from a user's list after unsubscribing */
void remove_topic_for_user(Client_topic **topic_list, Topic *topic_to_remove)
{
    Client_topic *head = *topic_list;

    /* Not subscribed to anything */
    if (head == NULL)
        return;
    
    /* unsubscribe first element */
    if (head->topic == topic_to_remove) {
        (*topic_list) = (*topic_list)->next;
        return;
    }

    /* just a normal element */
    while (head->next != NULL) {
        if (head->next->topic == topic_to_remove) {
            head->next = head->next->next;
            return;
        }
        head = head->next;
    }  
}

/* add or update data for the user with new_id
 * return 0 -> already connected and active
 * return 1 -> first time it connects
 * return 2 -> reconnected, just change the socket
 */
int add_tcp_client(Tcp_client **clients_list, char *new_id, int new_socket)
{
    Tcp_client **head=clients_list;

    while ((*head) != NULL && strcmp((*head)->id, new_id) != 0)
        head = &(*head)->next;
    
    if ((*head) != NULL) {

        if ((*head)->connected == 1) {
            return 0;
        }
        /* previously connected */
        (*head)->connected = 1;
        (*head)->client_socket = new_socket;

        int flag = 1;
        setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

        return 2;
    }

    /* first time the user with this id connects */
    (*head) = malloc(sizeof(Tcp_client));
    (*head)->connected = 1;
    (*head)->client_socket = new_socket;
    (*head)->id = calloc((strlen(new_id) + 1), sizeof(char));
    strcpy((*head)->id, new_id);
    (*head)->topic_element = NULL;
    (*head)->next = NULL;
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    return 1;
}

/* add a message for a topic, if the topic does not exist - create it */
void add_topic_message(Topic **topics_list, char *topic_name, char *mess, int mess_len, Tcp_client *all_clients)
{
    Topic **head = topics_list;

    /* go until the topic is found or to the end of the topic list */
    while ((*head) != NULL && strcmp((*head)->name, topic_name))
        head = &(*head)->next;
    /* add the topic with one initial message */
    if ((*head) == NULL) {
        (*head) = malloc(sizeof(Topic));
        (*head)->name = calloc(strlen(topic_name) + 1, sizeof(char));
        strcpy((*head)->name, topic_name);
        (*head)->no_messages_ever = 1;
        (*head)->next = NULL;
        (*head)->mess_head = malloc(sizeof(Message));
        (*head)->mess_head->identifier = (*head)->no_messages_ever;
        (*head)->mess_head->len = mess_len;
        (*head)->mess_head->message_value = calloc(2000, sizeof(char));
        memcpy((*head)->mess_head->message_value, mess, mess_len);
        (*head)->mess_head->no_sent_clients = 0;

        /* see for how many clients we should keep the message */
        (*head)->mess_head->no_possible_clients = 0;
        Tcp_client *copy = all_clients;
        Client_topic *this_topic;
        while (copy != NULL) {
            this_topic = user_has_topic(topic_name, &(copy->topic_element));
            if (this_topic != NULL) {
                if (copy->connected == 1 || this_topic->sf == 1) {
                    ((*head)->mess_head->no_possible_clients)++;
                }
            }
            copy = copy->next;
        }

        (*head)->mess_tail = (*head)->mess_head;
        (*head)->mess_head->next = (*head)->mess_tail->next = NULL;
    } else {
        /* the topic already exists, so just add message */
        ((*head)->no_messages_ever)++;
        Message **last = &((*head)->mess_tail);
        while ((*last) != NULL) {
            last = &(*last)->next;
        }
        (*last) = malloc(sizeof(Message));
        (*last)->identifier = (*head)->no_messages_ever;
        (*last)->len = mess_len;
        (*last)->message_value = calloc(2000, sizeof(char));
        memcpy((*last)->message_value, mess, mess_len);

        (*last)->no_sent_clients = 0;
        /* see for how many clients we should keep the message */
        (*last)->no_possible_clients =  0;
        Tcp_client *copy = all_clients;
        Client_topic *this_topic;
        while (copy != NULL) {
            this_topic = user_has_topic(topic_name, &(copy->topic_element));
            if (this_topic != NULL) {
                if (copy->connected == 1 || this_topic->sf == 1) {
                    ((*last)->no_possible_clients)++;
                }
            }
            copy = copy->next;
        }

        (*last)->next = NULL;
        (*head)->mess_tail = (*last);

        if ((*head)->no_messages_ever == 1 || ((*head)->mess_head == NULL)) {
            (*head)->mess_head = (*last);
        }
    }
}

/* a user subscribes to a topic that does not exist,
 * so we add it without a message
 */
void add_empty_topic(Topic **topics_list, char *topic_name)
{
    Topic **head = topics_list;
    while ((*head) != NULL) {
        head = &(*head)->next;
    }
    (*head) = malloc(sizeof(Topic));
    (*head)->name = calloc(strlen(topic_name) + 1, sizeof(char));
    strcpy((*head)->name, topic_name);
    (*head)->no_messages_ever = 0;
    (*head)->mess_tail = (*head)->mess_head = NULL;
    (*head)->next = NULL;
}

/* function that checks if a topic exists and return a pointer to it */
Topic *is_topic(char *name, Topic *topics_list)
{
    Topic *head = topics_list;

    while (head != NULL) {
        if (!strcmp(name, head->name)) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

/* get a client after its socket fd */
Tcp_client *get_client(int fd, Tcp_client *clients_list)
{
    Tcp_client *head = clients_list;

    while (head != NULL) {
        if (fd == head->client_socket)
            return head;
        head = head->next;
    }
    return NULL;
}

/* function used for debugging -> displays the topic each user is subscribed to */
void print_clients_subscriptions(Tcp_client **all_clients)
{
    Tcp_client *copy = *all_clients;
    while (copy != NULL) {
        printf("Client %s are topic-urile: ", copy->id);
        Client_topic *lst = copy->topic_element;
        while (lst != NULL) {
            printf("%s, ", lst->topic->name);
            lst = lst->next;
        }
        printf("\n");
        copy = copy->next;
    }
}

/* check if a message is redundant and remove it
 * basically, remove the message if we sent it to all the users that requested it
 */
void check_remove(Topic *topic, Message *mess)
{
    if (mess->no_possible_clients == mess->no_sent_clients) {
        Message *hd = topic->mess_head;
        /* the only message in the list */
        if ((topic->mess_head == topic->mess_tail) && (topic->mess_head == mess)) {
            topic->mess_tail = topic->mess_head = topic->mess_head->next;
            free(hd);
            return;
        }
        /* the first message in the list */
        if (hd == mess) {
            topic->mess_head = topic->mess_head->next;
            free(hd);
            return;
        }
        /* iterate through the list until we find the message */
        while (topic->mess_head->next != NULL) {
            if (topic->mess_head->next == mess) {
                Message *to_remove = topic->mess_head->next;
                topic->mess_head->next = topic->mess_head->next->next;
                topic->mess_head = hd;
                free(to_remove);
                return;
            }
            topic->mess_head = topic->mess_head->next;
        }
        topic->mess_head = hd;
    }
}

int main(int argc, char **argv)
{
    /* buffer for receiving data from stdin */
    char *stdin_buffer = malloc(50 * sizeof(char));
    /* storage buffer for a topic name */
    char *topic_name = malloc(50 * sizeof(char));
    /* list for all the clients */
    Tcp_client *all_clients = NULL;
    /* list for all the topics */
    Topic *all_topics = NULL;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    /* initialize a socket and a buffer for receiving from udp */
    int sock_udp = socket(PF_INET, SOCK_DGRAM, 0);
    char *udp_buffer = malloc(2000 * sizeof(char));
    if (sock_udp == -1) {
        perror("SOCK_FD UDP nevalid\n");
        exit(0);
    }
    int port = atoi(argv[1]);

    struct sockaddr_in serv_addr, udp_client_addr, tcp_client_addr;
    memset(&udp_client_addr, 0, sizeof(udp_client_addr));
    socklen_t len_udp = sizeof(udp_client_addr);
    memset(&tcp_client_addr, 0, sizeof(tcp_client_addr));
    socklen_t len_tcp = sizeof(tcp_client_addr);

    /* set server ip */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    /* bind */
    int ret = bind(sock_udp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        perror("Bind error udp\n");
        exit(0);
    }

    /* create tcp socket for new connections */
    char *tcp_buffer = malloc(200 * sizeof(char)); 
    memset(tcp_buffer, 0, 200);
    int sock_tcp_main = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_tcp_main < 0) {
        perror("SOCK_FD TCP nevalid");
        exit(0);
    }

    /* bind the tcp socket */
    ret = bind(sock_tcp_main, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        perror("Bind error tcp\n");
        exit(0);
    }
    
    /* mark the socket as a listener */
    if ((listen(sock_tcp_main, 10)) != 0) {
        printf("Listen failed\n");
        exit(0);
    }

    /* add I/O multiplex */
    struct pollfd *pfds = malloc(50 * sizeof(struct pollfd));
    int nfds = 0;
    int reach = 50;

    /* add stdin as listener socket */
    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds++].events = POLLIN;

    /* add udp listener socket */
    pfds[nfds].fd = sock_udp;
    pfds[nfds++].events = POLLIN;

    /* add tcp listener socket */
    pfds[nfds].fd = sock_tcp_main;
    pfds[nfds++].events = POLLIN;

    int flag = 1;
    setsockopt(sock_tcp_main, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));


    while (1) {
        poll(pfds, nfds, -1);

        if ((pfds[0].revents & POLLIN) != 0) {
            /* parse data from stdin */
            memset(stdin_buffer, 0, 50);

            /* read the data in stdin_buffer */
            if ((ret = read(pfds[0].fd, stdin_buffer, 50)) < 0) {
                perror("[SERV] couldn't read from stdin\n");
                exit(0);
            }
            if(ret == 0) {
                perror("[SERV] stdin closed connection\n");
                exit(0);
            }
            /* only allow the input exit */
            if (!strcmp(stdin_buffer, "exit\n")) {
                /* close the server and all clients */
                for (int i = 3; i < nfds; i++) {
                    memset(tcp_buffer, 0, 200);
                    strcpy(tcp_buffer, "Exit server.");
                    ret = my_send(pfds[i].fd, tcp_buffer, strlen(tcp_buffer));
                    if (ret < 0)
                        perror("[SERV] couldn't close clients.\n");
                    close(pfds[i].fd);
                }
                close(pfds[0].fd);
                close(pfds[1].fd);
                close(pfds[2].fd);
                return 0;
            } else {
                /* just a junky print for invalid inputs */
                printf("S-a primit de la stdin: %s\n", stdin_buffer);
            }

        } else if ((pfds[1].revents & POLLIN) != 0) {
            /* parse data from udp */

            int bytes_from = recvfrom(sock_udp, udp_buffer, 2000, MSG_WAITALL, (struct sockaddr *)&udp_client_addr, &len_udp);
            if (bytes_from <= 0) {
                perror("[SERV] Recvfrom udp failed\n");
            }
            udp_buffer[bytes_from] = '\0';

            /* change a character after 50 bytes to make sure we can operate on strings with the topic name */
            char save = udp_buffer[50];
            udp_buffer[50] = '\0';
            strcpy(topic_name, udp_buffer);
            udp_buffer[50] = save;

            /* encapsulate udp client data and the data itself in a single buffer */
            char *full_message = calloc(2000, sizeof(char));
            memcpy(full_message, &udp_client_addr, len_udp);
            memcpy(full_message + len_udp, udp_buffer, bytes_from);

            /* add it in the Messages list */
            add_topic_message(&all_topics, topic_name, full_message, bytes_from + len_udp, all_clients);

            /* check if we can immediately send it to someone */
            Tcp_client *copy = all_clients;
            Client_topic *topic_to_send;
            while (copy != NULL) {
                topic_to_send = user_has_topic(topic_name, &(copy->topic_element));
                if (topic_to_send != NULL) {
                    Message *fst = topic_to_send->topic->mess_head;
                    while (fst != NULL) {
                        if (fst->identifier > topic_to_send->last_message_identifier) {
                            if (copy->connected == 1) {
                                ret = my_send(copy->client_socket, fst->message_value, fst->len);
                                if (ret < 0)
                                    perror("[SERV] Direct send after udp recv failed\n");
                                fst->no_sent_clients++;
                                topic_to_send->last_message_identifier = fst->identifier;
                            } else {
                                if (topic_to_send->sf == 0)
                                    topic_to_send->last_message_identifier = fst->identifier;
                            }
                        }
                        Message *old = fst;
                        fst = fst ->next;
                        check_remove(topic_to_send->topic, old);
                    }
                }
                copy = copy->next;
            }
            
            memset(udp_buffer, 0, 2000);

        } else if ((pfds[2].revents & POLLIN) != 0) {
            /* handle new tcp connection */
            int sock_new_client = accept(pfds[2].fd, (struct sockaddr*)&tcp_client_addr, &len_tcp);
            if (sock_new_client < 0) {
                perror("[SERV] Accept failed\n");
                exit(-1);
            }

            /* get client id */
            if (my_recv(sock_new_client, tcp_buffer) < 0) {
                perror("[SERV] didn't receive client id\n");
                exit(-1);
            }

            /* Accept succeeded */
            if (nfds == reach) {
                pfds = realloc(pfds, 2 * reach * sizeof(struct pollfd));
                reach *= 2;
            }
            pfds[nfds].fd = sock_new_client;
            pfds[nfds++].events = POLLIN;

            int ok = add_tcp_client(&all_clients, tcp_buffer, sock_new_client);
            if (ok) {
                printf("New client %s connected from %s:%d.\n", tcp_buffer,
                    inet_ntoa(tcp_client_addr.sin_addr), ntohs(tcp_client_addr.sin_port));
                if (ok == 2) {
                    /* client reconnected - send messages on sf topics */
                    Tcp_client *clnt = all_clients;
                    while (clnt != NULL) {
                        if (clnt->client_socket == sock_new_client) {
                            Client_topic *topics = clnt->topic_element;
                            while (topics != NULL) {
                                Message *fst = topics->topic->mess_head;
                                while (fst != NULL) {
                                    if (fst->identifier > topics->last_message_identifier) {
                                        ret = my_send(clnt->client_socket, fst->message_value, fst->len);
                                        if (ret < 0)
                                            perror("[SERV] couldn't send sf messages\n");
                                        fst->no_sent_clients++;
                                        topics->last_message_identifier = fst->identifier;
                                    }
                                    Message *old = fst;
                                    fst = fst ->next;
                                    check_remove(topics->topic, old);
                                }
                                topics = topics->next;
                            }
                        }
                        clnt = clnt->next;
                    }
                    
                }
            }
            else {
                /* A client with the same id is still connected */
                char *nume_cl = calloc(200, sizeof(char));
                strcpy(nume_cl, tcp_buffer);
                memset(tcp_buffer, 0, 200);
                sprintf(tcp_buffer, "Client %s already connected.", nume_cl);

                /* send a message so that the client will close itself */
                ret = my_send(sock_new_client, tcp_buffer, strlen(tcp_buffer));
                if (ret < 0)
                    perror("[SERV] couldn't send already connected message\n");
                puts(tcp_buffer);
                free(nume_cl);

                /* close the connection */
                close(sock_new_client);
            }
            memset(tcp_buffer, 0, 200);
            
        } else {
            /* handle message from clients */
            for (int i = 3; i < nfds; i++) {

                /* check from which client we have received the message */
                if ((pfds[i].revents & POLLIN) != 0) {
                    if (recv(pfds[i].fd, tcp_buffer, 200, 0) < 0) {
                        perror("[SERV] Couldn't receive from tcp client\n");
                        return -1;
                    }
                    char *token;
                    char *safe_copy = malloc(200 * sizeof(char));
                    strcpy(safe_copy, tcp_buffer);

                    /* check if we got a subscribe message */
                    if (strstr(tcp_buffer, "subscribe ") == tcp_buffer) {

                        /* separate the subscribe message */
                        token = strtok(safe_copy, " \n");
                        token = strtok(NULL, " \n");
                        char *topic_nm = malloc(200 * sizeof(char));
                        strcpy(topic_nm, token);

                        /* add topic if necessary */
                        if (is_topic(token, all_topics) == NULL)
                            add_empty_topic(&all_topics, token);
                        token = strtok(NULL, " \n");

                        /* try subscribing to topic */
                        if (user_has_topic(topic_nm, &(get_client(pfds[i].fd, all_clients)->topic_element)) == NULL) {
                            add_topic_for_user(&(get_client(pfds[i].fd, all_clients)->topic_element), token[0] - '0', is_topic(topic_nm, all_topics));
                            memset(tcp_buffer, 0, 200);

                            /* send some sort of ack, subscribe succeeded */
                            sprintf(tcp_buffer, "Subscribed to topic.");
                            ret = my_send(pfds[i].fd, tcp_buffer, strlen(tcp_buffer));
                            if (ret < 0)
                                perror("[SERV] Unable to send subscribe confirmation\n");
                        } else {
                            memset(tcp_buffer, 0, 200);

                            /* send already subscribed message */
                            sprintf(tcp_buffer, "Already subscribed to %s.", topic_nm);
                            ret = my_send(pfds[i].fd, tcp_buffer, strlen(tcp_buffer));
                            if (ret < 0)
                                perror("[SERV] Unable to send already subscribed confirmation\n");
                        }

                    } else if (strstr(tcp_buffer, "unsubscribe ") == tcp_buffer) {
                        
                        /* unsubscribe user from a topic */
                        token = strtok(safe_copy, " \n");
                        token = strtok(NULL, " \n");

                        /* check if the topic exists */
                        if (is_topic(token, all_topics) != NULL) {
                            if (user_has_topic(token, &(get_client(pfds[i].fd, all_clients)->topic_element)) != NULL) {
                                /* if user was subscribed to the topic, send ack */

                                remove_topic_for_user(&(get_client(pfds[i].fd, all_clients)->topic_element), is_topic(token, all_topics));
                                memset(tcp_buffer, 0, 200);
                                sprintf(tcp_buffer, "Unsubscribed from %s.", token);
                                ret = my_send(pfds[i].fd, tcp_buffer, strlen(tcp_buffer));
                                if (ret < 0)
                                    perror("[SERV] Unable to send unsubscribe confirmation\n");
                            } else {
                                memset(tcp_buffer, 0, 200);

                                /* The topic exists, but the user is not subscribed to it */
                                sprintf(tcp_buffer, "User is not subscribed to %s.", token);
                                ret = my_send(pfds[i].fd, tcp_buffer, strlen(tcp_buffer));
                                if (ret < 0)
                                    perror("[SERV] Unable to send unsubscribe confirmation\n");
                            }
                        } else {
                            /* The topic doesn't even exists */
                            memset(tcp_buffer, 0, 200);
                            sprintf(tcp_buffer, "Topic %s does not exist.", token);
                            ret = my_send(pfds[i].fd, tcp_buffer, strlen(tcp_buffer));
                            if (ret < 0)
                                perror("[SERV] Unable to send topic does not exist\n");
                        }                       

                    } else if (strstr(tcp_buffer, "exit") == tcp_buffer) {
                        /* a client disconnected, interchange with the last fd */

                        Tcp_client *copy = all_clients;
                        while (copy != NULL) {
                            if (copy->client_socket == pfds[i].fd) {
                                memset(tcp_buffer, 0, 200);
                                printf("Client %s disconnected.\n", copy->id);
                                copy->connected = 0;
                                break;
                            }
                            copy = copy->next;
                        }

                        close(pfds[i].fd);
                        /* reduce the number of fds which we are considering valid */
                        nfds--;
                        pfds[i].fd = pfds[nfds].fd;
                    }
                    memset(tcp_buffer, 0, 200);
                }
            }
        }
    }
    return 0;
}