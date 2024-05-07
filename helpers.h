#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;

#define MAX_CONNECTIONS    32
#define MAX_BUF_SIZE 1600
#define ID_MAX_LEN 11
#define SUB_LEN 15
#define TOPIC_LEN 50


/*
    Check the status of the TCP client
    0 - first time connectiong
    1 - reconnecting
    2 - client already connected
*/
enum States {
    FIRST_TIME,
    RECONNECT,
    CONNECTED
};

/*
 * Macro de verificare a erorilor
 * Exemplu:
 * 		int fd = open (file_name , O_RDONLY);
 * 		DIE( fd == -1, "open failed");
 */

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)


#define SERVER_LOCALHOST_IP "127.0.0.1"

/*
  The data structure that keeps a TCP client that was connected
  at the server and the data necesary about it
*/
struct tcp_client {
    int fd;
    bool connected;
    char id[11];  // Assuming ID is a string of 10 characters + null terminator
    vector<pair<char *, int>> subscriptions;  // Change to std::string from char*
};

/*
  The packet that UDP clients send to the server.
  It includes topic, data_type and the payload of data.
*/
struct udp_packet {
  char topic[51];
  unsigned int data_type;
  char payload[1501];
};

#endif
