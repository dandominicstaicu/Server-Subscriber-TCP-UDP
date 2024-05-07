/*
 * Protocoale de comunicatii
 * Schelet Laborator 7 - TCP
 * Echo Server
 */

#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/*
    receiving exactly len bytes from buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    void *buff = buffer;

    while (bytes_remaining)
    {
        int bytes = recv(sockfd, buff, bytes_remaining, 0);

        // error
        if (bytes < 0)
            return bytes;

        // nothing left to write in the buffer
        if (bytes == 0)
            break;

        bytes_received += bytes;
        bytes_remaining -= bytes;

        // cast void* to char* so it can add
        char *charPtr = static_cast<char *>(buff);
        charPtr += bytes;
        // Cast char* back to void*
        buff = static_cast<void *>(charPtr);
    }

    return bytes_received;
}

/*
    sending exactly len bytes from buffer
*/

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    void *buff = buffer;

    while (bytes_remaining)
    {
        int bytes = send(sockfd, buff, bytes_remaining, 0);

        // error
        if (bytes < 0)
            return bytes;

        // noting left to write in the buffer
        if (bytes == 0)
            break;

        bytes_sent += bytes;
        bytes_remaining -= bytes;
        
        // Cast void* to char* deoarece so it can add
        char *charPtr = static_cast<char *>(buff);
        charPtr += bytes;
        // Cast char* back to void*
        buff = static_cast<void *>(charPtr);
    }

    return bytes_sent;
}
