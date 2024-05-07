/*
 * Protocoale de comunicatii
 * Schelet Laborator 7 - TCP
 * Echo Server
 * server.c
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <queue>
#include <string>
#include <unordered_map>
#include <cmath>
#include <algorithm>

#include "common.h"
#include "helpers.h"





/* an array with all the TCP clients that were ever connected */
struct tcp_client tcp_clients[MAX_CONNECTIONS];
int num_tcp_clients = 0;

unordered_map<string, queue<string>> packets_queue;

/* Create the poll of descriptors */
struct pollfd poll_fds[MAX_CONNECTIONS];
int num_clients = 3;

void setup_fds(int tcp_sockfd, int udp_sockfd) {
    poll_fds[0].fd = tcp_sockfd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = udp_sockfd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = 0;
    poll_fds[2].events = POLLIN;
}

void tcp_first_connect(int newsockfd, char *id) {
    tcp_clients[num_tcp_clients].fd = newsockfd;
    tcp_clients[num_tcp_clients].connected = true;
    strcpy(tcp_clients[num_tcp_clients].id, id);
    num_tcp_clients++;
}

void tcp_reconnect(int newsockfd, int tcp_pos, const char *id) {
    /* update with the newly created fd and mark it as connected */
    tcp_clients[tcp_pos].connected = true;
    tcp_clients[tcp_pos].fd = newsockfd;

    /*
     * send from the queue the packets that were received while the client
     * was disconnected if it had store forward set to true
     */
    while (!packets_queue[id].empty()) {
        int ret = send_all(newsockfd, &packets_queue[id].front()[0], MAX_BUF_SIZE);
        DIE(ret < 0, "[SERV] send");
        packets_queue[id].pop();
    }
}

bool match_topic(const char *subscription, const char *topic) {
    while (*subscription != '\0' && *topic != '\0') {
        if (*subscription == '*') {
            if (*(subscription + 1) == '\0') // '*' at the end matches all remaining characters
                return true;
            subscription++;
            if (*topic == '\0') // If topic ends but subscription still has '*', no match
                return false;
            while (*topic != '\0') { // Try all possible matches
                if (match_topic(subscription, topic))
                    return true;
                topic++;
            }
            return false; // No match found
        } else if (*subscription == '+') {
            if (*(subscription + 1) != '/' &&
                *(subscription + 1) != '\0') // Ensure '+' is followed by '/' or end of string
                return false;
            const char *next_slash = strchr(topic, '/');
            if (next_slash != nullptr) { // '+' matches until next '/'
                topic = next_slash;
            } else { // If no '/' found, match till end if '+' is at end of subscription
                if (*(subscription + 1) == '\0')
                    topic += strlen(topic); // Move to end of topic
                else
                    return false; // If '+' not at end and no '/', no match
            }
            subscription++;
        } else if (*subscription == *topic) {
            subscription++;
            topic++;
        } else {
            return false;
        }
    }
    // Check for exact end match
    return *subscription == '\0' && *topic == '\0';
}

/*
    Check if a topic is in the list of subscribers of a client:
    if it's not subscribed, return 0
    if it's subscribed without store-forward, return 1
    if it's subscribed with store-forward, return 2
*/
bool is_more_inclusive(const char *new_sub, const char *existing_sub) {
    // Simple check: if new_sub ends with '*' and existing_sub ends with '+'
    size_t new_len = strlen(new_sub);
    size_t existing_len = strlen(existing_sub);
    if (new_sub[new_len - 1] == '*' && existing_sub[existing_len - 1] == '+') {
        // Compare the base part of the subscription without the last character
        return strncmp(new_sub, existing_sub, new_len - 1) == 0;
    }
    return false;
}

bool update_subscriptions(vector<pair<char *, int>> &subscriptions, const char *topic, int sf) {
    // Check if a more inclusive subscription exists
    for (auto &sub: subscriptions) {
        if (is_more_inclusive(topic, sub.first)) {
            // Replace existing subscription with more inclusive one
            free(sub.first);  // Assume dynamic allocation
            sub.first = strdup(topic);
            sub.second = sf;
            return true;  // Return true as the subscription list was updated
        }
    }

    // Check if the exact same subscription already exists
    for (auto &sub: subscriptions) {
        if (strcmp(sub.first, topic) == 0 && sub.second == sf) {
            return false;  // Subscription already exists with the same settings
        }
    }

    // If no more inclusive or exact same subscription is found, add the new one
    subscriptions.push_back({strdup(topic), sf});
    return true;  // Return true as a new subscription was added
}


int check_subscribed(const vector<pair<char *, int>> &subscriptions, const char *topic) {
    for (auto &sub: subscriptions)
        if (strcmp(sub.first, topic) == 0 || match_topic(sub.first, topic))
            return 1 + sub.second;

    return 0;
}

void subscribe(int index, char *topic, int sf) {
    bool subscribed = false;

    // Find the client who sent the request (by file descriptor)
    for (int k = 0; k < num_tcp_clients; k++) {
        if (tcp_clients[k].fd == poll_fds[index].fd) {
            // Update subscriptions; this function now also checks for existing subscriptions
            subscribed = update_subscriptions(tcp_clients[k].subscriptions, topic, sf);

            if (subscribed) {
                char buf[MAX_BUF_SIZE] = {0};

                // Send a confirmation message to the client
                sprintf(buf, "Subscribed to topic %s.\n", topic);
                int send_result = send_all(poll_fds[index].fd, &buf, MAX_BUF_SIZE);
                DIE(send_result < 0, "[SERV] send");
            } else {
                fprintf(stderr, "Already subscribed to topic %s\n", topic);
            }
            break;
        }
    }
}


/*
    Create an udp_packet from the read buffer from the UDP socket
*/
struct udp_packet create_recv_packet(char *buffer) {
    struct udp_packet received_packet{};

    /* Extract the topic */
    memcpy(received_packet.topic, buffer, 50);
    received_packet.topic[50] = '\0';

    /* Store data_type */
    received_packet.data_type = (unsigned int) (unsigned char) buffer[50];

    /* Parse payload based on the data_type */
    switch (received_packet.data_type) {
        case 0: // INT
        {
            int32_t raw_int;
            memcpy(&raw_int, buffer + 52, sizeof(int32_t));
            unsigned int message = ntohl(raw_int);

            if (buffer[51] == 1) {
                /* if sign byte is 1, then it's negative */
                message = -message;
            }

            snprintf(received_packet.payload, sizeof(received_packet.payload), "%d", message);
        }
            break;

        case 1: // SHORT_REAL
        {
            uint16_t raw_short;
            memcpy(&raw_short, buffer + 51, sizeof(uint16_t));
            double message = (double) ntohs(raw_short) / 100.0;

            snprintf(received_packet.payload, sizeof(received_packet.payload), "%.2f", message);
        }
            break;

        case 2: // FLOAT
        {
            int32_t raw_float;
            memcpy(&raw_float, buffer + 52, sizeof(int32_t));
            auto value = (double) ntohl(raw_float);
            double power = pow(10, buffer[56]);

            if (buffer[51] == 1) {
                /* if sign byte is 1, then it's negative */
                value = -value;
            }

            double message = value / power;
            snprintf(received_packet.payload, sizeof(received_packet.payload), "%f", message);
        }
            break;

        case 3: // STRING
            strcpy(received_packet.payload, buffer + 51);
            break;

        default:
            fprintf(stderr, "Unrecognized data type\n");
            break;
    }

    return received_packet;
}

/* transform the received info to a string in the buffer */
void convert_to_string(sockaddr_in client_addr, udp_packet received_packet, char *output_buffer, size_t buffer_size) {
    /* datatype as strings */
    char const *data_type_string[] = {"INT", "SHORT_REAL", "FLOAT", "STRING"};

    char *inet_addr = inet_ntoa(client_addr.sin_addr);
    unsigned short port = ntohs(client_addr.sin_port);

    memset(output_buffer, 0, buffer_size);
    snprintf(output_buffer, buffer_size, "%s:%d - %s - %s - %s\n", inet_addr, port,
             received_packet.topic, data_type_string[received_packet.data_type], received_packet.payload);
}


void unsubscribe(int index, char *topic) {
    bool unsubscribed = false;

    for (int k = 0; k < num_tcp_clients; k++) {
        if (tcp_clients[k].fd == poll_fds[index].fd) {
            auto &subscriptions = tcp_clients[k].subscriptions;
            auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                                   [topic](const pair<char *, int> &p) { return !strcmp(p.first, topic); });

            if (it != subscriptions.end()) {
                subscriptions.erase(it);
                unsubscribed = true;
                break;
            }
        }
    }

    if (unsubscribed) {
        char buf[MAX_BUF_SIZE] = {0};

        sprintf(buf, "Unsubscribed from topic.\n");
        int ret = send_all(poll_fds[index].fd, &buf, MAX_BUF_SIZE);
        DIE(ret < 0, "[SERV] send");
    } else {
        fprintf(stderr, "Client is not subscribed to topic %s.\n", topic);
    }
}

void get_id_and_num(char *&id, int &client_num, struct pollfd poll_fd) {
    for (int k = 0; k < num_tcp_clients; k++) {
        if (tcp_clients[k].fd == poll_fd.fd) {
            id = tcp_clients[k].id;
            client_num = k;
            break;
        }
    }
}

void tcp_close_connection(int idx) {
    /* get the ID of the TCP client (knowing just the fd) */
    char *id;
    int client_num;

    get_id_and_num(id, client_num, poll_fds[idx]);

    printf("Client %s disconnected.\n", id);

    /* close the socket */
    close(poll_fds[idx].fd);

    /* remove the socket from the poll */
    for (int j = idx; j < num_clients - 1; j++)
        poll_fds[j] = poll_fds[j + 1];
    num_clients--;

    /* Mark the TCB subscriber as disconnected */
    tcp_clients[client_num].connected = false;
    tcp_clients[client_num].fd = -1;
}

void close_socket(int sock_fd) {
    char buf[MAX_BUF_SIZE] = {0};

    /* Send a close request to the client */
    sprintf(buf, "exit");
    int ret = send_all(sock_fd, &buf, sizeof(buf));
    DIE(ret < 0, "[SERV] send");

    /* close the socket */
    close(sock_fd);
}

void run_server(int tcp_sockfd, int udp_sockfd) {
    /* Listen for clients */
    int ret = listen(tcp_sockfd, MAX_CONNECTIONS);
    DIE(ret < 0, "[SERV] Error while listening");

    setup_fds(tcp_sockfd, udp_sockfd);

    while (true) {
        /* Poll the fds until can read from one of them */
        ret = poll(poll_fds, num_clients, -1);
        DIE(ret < 0, "[SERV] poll");

        for (int i = 0; i < num_clients; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == tcp_sockfd) {
                    /* New TCP connection */
                    sockaddr_in client_addr{};
                    socklen_t client_addr_len = sizeof(sockaddr_in);
                    int new_sock_fd = accept(tcp_sockfd, (sockaddr *) &client_addr, &client_addr_len);
                    DIE(new_sock_fd < 0, "[SERV] Error while accepting new TCP connection");

                    char id[ID_MAX_LEN];
                    ret = recv(new_sock_fd, &id, sizeof(id), 0);
                    DIE(ret < 0, "[SERV] Error while receiving id");

                    /*
                        Check the status of the TCP client
                        0 - first time connectiong
                        1 - reconnecting
                        2 - client already connected
                    */
                    States status = FIRST_TIME;
                    int tcp_pos = -1;

                    for (int j = 0; j < num_tcp_clients; ++j) {
                        if (strcmp(tcp_clients[j].id, id) == 0) {
                            tcp_pos = j;
                            status = RECONNECT;
                            if (tcp_clients[j].connected)
                                status = CONNECTED;
                            break;
                        }
                    }

                    /* if is already connected */
                    if (status == CONNECTED) {
                        printf("Client %s already connected.\n", id);
                        close_socket(new_sock_fd);
                        break;
                    }

                    /* Not already connected ->
                     * Add the new socket to the poll */
                    poll_fds[num_clients].fd = new_sock_fd;
                    poll_fds[num_clients].events = POLLIN;
                    num_clients++;

                    char *inet_addr = inet_ntoa(client_addr.sin_addr);
                    unsigned short port = ntohs(client_addr.sin_port);
                    printf("New client %s connected from %s:%d.\n",
                           id, inet_addr, port);

                    /* this is the first time connecting */
                    if (status == FIRST_TIME) {
                        tcp_first_connect(new_sock_fd, id);
                    } else { /* reconnecting */
                        tcp_reconnect(new_sock_fd, tcp_pos, id);
                    }

                    break;
                } else if (poll_fds[i].fd == udp_sockfd) {
                    char buf[MAX_BUF_SIZE] = {0};

                    struct sockaddr_in client_addr{};
                    socklen_t clen = sizeof(client_addr);

                    /* Receive the UDP packet */
                    ret = recvfrom(udp_sockfd, &buf, MAX_BUF_SIZE, 0,
                                   (struct sockaddr *) &client_addr, &clen);
                    DIE(ret < 0, "[SERV] receive UDP error");

                    /* Convert from char to a UDP struct */
                    udp_packet received_packet = create_recv_packet(buf);

                    /* Send the packet to the TCP clients which are subscribed to the given topic */
                    for (int k = 0; k < num_tcp_clients; k++) {
                        /* Check if the TCP client is subscribed to the topic of the UDP msg */
                        int sub = check_subscribed(tcp_clients[k].subscriptions, received_packet.topic);
                        if (sub > 0) {
                            convert_to_string(client_addr, received_packet, buf, MAX_BUF_SIZE);

                            /* if the client is connected, send the string */
                            if (tcp_clients[k].connected) {
                                ret = send_all(tcp_clients[k].fd, &buf, sizeof(buf));
                                DIE(ret < 0, "[SERV] send");
                            } else if (sub == 2) {
                                /* if the client is no longer connected, but the store-forward is set,
                                 * save the string in the message queue of the client in case it
                                 * will reconnect later */
                                packets_queue[tcp_clients[k].id].push(buf);
                            }
                        }
                    }

                    break;
                } else if (poll_fds[i].fd == 0) { // Check for a message from stdin
                    /* Clear the buffer and read the message */
                    char buf[MAX_BUF_SIZE] = {0};

                    fgets(buf, sizeof(buf), stdin);

                    /* Handle 'exit' command */
                    if (strncmp(buf, "exit", 4) == 0) {
                        // Iterate through all TCP clients to send 'exit' and close their sockets
                        for (int idx = 0; idx < num_tcp_clients; idx++) {
                            if (tcp_clients[idx].connected) {
                                close_socket(tcp_clients[idx].fd);
                            }
                        }

                        /* Exit to main to close listening sockets and stop the server */
                        return;
                    } else {
                        fprintf(stderr, "Unrecognized command.\n");
                    }

                    break;
                } else {
                    /*
                     * if data is received on any socket
                     * than receive the message
                     */
                    char buf[MAX_BUF_SIZE] = {0};

                    ret = recv_all(poll_fds[i].fd, &buf, MAX_BUF_SIZE);
                    DIE(ret < 0, "[SERV] recv error");

                    /* TCP client closed connection */
                    if (ret == 0) {
                        tcp_close_connection(i);
                    } else {
                        /*
                         * If the client is still connected, extract from the recv string
                         * subscribe/unsubscribe status
                         * the topic
                         * store forward
                         */
                        char sub[SUB_LEN] = {0};
                        char topic[TOPIC_LEN] = {0};
                        int sf;

                        sscanf(buf, "%s %s %d", sub, topic, &sf);

                        // If a subscribe request is received
                        if (!strcmp(sub, "subscribe")) {
                            subscribe(i, topic, sf);
                        } else if (!strcmp(sub, "unsubscribe")) {
                            /* If an unsubscribe request is received */
                            unsubscribe(i, topic);
                        } else {
                            fprintf(stderr, "Unrecognized command.\n");
                        }
                    }

                    break;
                }
            }
        }
    }
}


int main(int argc, char *argv[]) {
    uint16_t port;
    int udp_sockfd, tcp_sockfd;
    int flag = 1;
    struct sockaddr_in server_addr{};


    /* Deactivate buffering for stdout */
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    /* Check the number of arguments */
    DIE(argc != 2, "[SERV] Usage: ./server <PORT_SERVER>");

    /* Get the server port */
    int ret = sscanf(argv[1], "%hu", &port);
    DIE(ret != 1, "[SERV] Given port is invalid");

    /* Create UDP socket */
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "[SERV] Error while creating UDP socket");

    /* Create TCP socket */
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "[SERV] Error while creating TCP socket");

    /* Disable the Nagle algorithm */
    ret = setsockopt(tcp_sockfd, IPPROTO_TCP, SO_REUSEADDR | TCP_NODELAY, &flag, sizeof(int));
    DIE (ret < 0, "[SERV] Error while disabling the Nagle algorithm");

    ret = setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    DIE(ret < 0, "[SERV ]setsockopt(SO_REUSEADDR) udp failed");

    /* Set port and IP that we'll be listening for */
    memset(&server_addr, 0, sizeof(server_addr));
    ret = inet_pton(AF_INET, SERVER_LOCALHOST_IP, &server_addr.sin_addr.s_addr);
    DIE(ret <= 0, "inet_pton");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    /* Bind to the set port and IP */
    ret = bind(udp_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    DIE(ret < 0, "[SERV] Couldn't bind to the port");

    ret = bind(tcp_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    DIE(ret < 0, "[SERV] Couldn't bind to the port");

    run_server(tcp_sockfd, udp_sockfd);

    return 0;
}
