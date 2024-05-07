#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

#define MAX_BUF_SIZE 1600

void initialize_poll_fds(struct pollfd *poll_fds, int sockfd) {
    poll_fds[0].fd = 0;  // Standard input
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = sockfd;  // Socket file descriptor
    poll_fds[1].events = POLLIN;
}

void cleanup_and_exit(int sockfd, int exit_code) {
    close(sockfd);  // Close the socket
    exit(exit_code);  // Exit with the given code
}

/* wait for packets from stdin or from server */
void run_client(int sockfd) {
    /* Array of poll file descriptors for multiplexing */
    struct pollfd poll_fds[2];
    int num_clients = 2;

    initialize_poll_fds(poll_fds, sockfd);

    /* Loop indefinitely to handle input from stdin or server */
    while (true) {
        /* Perform the poll operation with no timeout (-1) */
        int ret = poll(poll_fds, num_clients, -1);
        DIE(ret < 0, "[SUB] poll failed");

        /* Iterate over file descriptors to check which one is ready*/
        for (int i = 0; i < num_clients; i++) {
            /* Check if current file descriptor has any event */
            if (poll_fds[i].revents & POLLIN) {
                /* Check if the event is on stdin */
                if (poll_fds[i].fd == 0) {
                    char buf[MAX_BUF_SIZE] = {0};

                    /* Read input from stdin */
                    fgets(buf, sizeof(buf), stdin);

                    /* Check if the command is 'exit' */
                    if (!strncmp(buf, "exit", 4)) {
                        cleanup_and_exit(sockfd, 0);
                    }

                    /* Otherwise, send the data read from stdin to the server */
                    send_all(sockfd, buf, MAX_BUF_SIZE);
                } else {
                    char buf[MAX_BUF_SIZE] = {0};

                    ret = recv_all(sockfd, buf, MAX_BUF_SIZE);
                    if (ret <= 0) {
                        cleanup_and_exit(sockfd, 1);
                    }

                    /* Check for 'exit' command from server */
                    if (!strncmp(buf, "exit", 4)) {
                        cleanup_and_exit(sockfd, 0);
                    }

                    /* Print the message received from the server to stdout */
                    printf("%s", buf);
                }
            }
        }
    }
}

void initialize_socket_and_connect(const char *ip, uint16_t port, int &sockfd) {
    /* Initialize sockaddr_in with server address */
    struct sockaddr_in serv_addr{};
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    /* Convert IP string to binary form */
    int ret = inet_pton(AF_INET, ip, &serv_addr.sin_addr.s_addr);
    DIE(ret <= 0, "[SUB] inet_pton failed");

    /* Create a socket and check for errors */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "[SUB] socket creation failed");

    /* Connect to the server */
    ret = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "[SUB] connection failed");
}

int main(int argc, char *argv[]) {
    // Disable output buffering
    setvbuf(stdout, nullptr, _IONBF, BUFSIZ);

    /* Check command line arguments */
    /* Check the number of arguments */
    DIE(argc != 4, "[SUB] Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

    /* Check if the ID_CLIENT and PORT_SERVER are valid*/
    DIE(strlen(argv[1]) > 10, "[SUB] Given ID is invalid");
    DIE(atoi(argv[3]) == 0, "[SUB] Given port is invalid");

    /* Parse the port number */
    uint16_t port;
    int ret = sscanf(argv[3], "%hu", &port);
    DIE(ret != 1, "[SUB] invalid port number");

    /* Setup socket and connect */
    int sockfd;
    initialize_socket_and_connect(argv[2], port, sockfd);

    /* Send the client's ID to the server */
    send_all(sockfd, argv[1], strlen(argv[1]) + 1);

    /* Enter the main client run loop */
    run_client(sockfd);

    /* close socket before exiting */
    close(sockfd);
    return 0;
}