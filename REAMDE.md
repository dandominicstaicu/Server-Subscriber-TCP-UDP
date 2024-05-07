# Subscriber-Server TCP UDP Application


### Copyright
**Dan-Dominic Staicu**
**321CAb**

## Description

- TCP UDP client-server application for handling messages.
- A server that receives messages from UDP clients and retransmits them to the connected TCP clients that are subscribed to the topic.
- The server establishes a communication between the clients allowing message publication and subscription/unsubscription
- TCP clients can subscribe/unsubscribe from topics and display messages.
- UDP clients publish messages to the server.
- Sockets and I/O multiplexing are implemented using the poll() in the server and the TCP client.
- The server implements store-and-forward so the TCP clients receive messages when they are offline.

## Structure

- *server.cpp*: the code for the server
- *subscriber.cpp*: the TCP client
- *common.cpp*: wrappers over recv and send from the sockets livrary
- *helpers.h*: macros, structures, defines, enums

## Running the application
### Run the Server

``./server <PORT>``

### Run a TCP Subscriber

``./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>``


## Implementation

The project builds upon a robust TCP protocol framework, integrating multiplexing with poll() and the TCP socket API, which is crucial for handling multiple TCP client connections to the server. In the common.cpp file, the functions recv_all and send_all are included to ensure that all bytes are completely sent and received by the send and recv functions, addressing the issue that these functions may transfer fewer bytes than requested.

The software structure is extended with the helpers.h, common.cpp, and common.h files, and further developed in server.cpp and subscriber.cpp files to establish the core functionality of both server and subscriber clients.

For the integration of UDP clients, the solution incorporates UDP socket API management. The server is configured to receive datagrams from UDP clients, capturing messages as strings and parsing them into a udp_packet structure (defined in helpers.h). A dedicated function was developed to manage this parsing process, following specific instructions based on the data type received.

The server design also incorporates the ability to manage TCP client subscriptions to topics, allowing for complex pattern matching using wildcards. This functionality enables clients to subscribe using wildcards like * or +, where * can match multiple levels and + matches only one level of a topic structure, akin to paths in a filesystem.

Specifically, the match_topic function is crucial as it allows the server to determine whether a topic from a UDP client matches a subscription pattern held by a TCP client. This function deals with the complexity of wildcards effectively. If a subscription ends with *, it can match all following characters of the topic, facilitating broad message dissemination based on coarse criteria. Conversely, + is used to match a single hierarchy in a topic path, ending at the next slash (/) or the end of the string, allowing for more fine-grained control over the message flow.

When UDP messages arrive at the server, they are parsed, and the topic is extracted. The server then uses this match_topic function to evaluate whether the message's topic aligns with any of the active subscriptions from TCP clients. If a match is found, the message is forwarded to the corresponding clients based on their subscription preferences. This might involve immediate message forwarding to connected clients or storing the message for future delivery if the client is temporarily disconnected but has subscribed with the store-forward option.

To manage client interactions such as subscriptions, a tcp_client structure was established (in helpers.h) to store critical data about each TCP client, including their ID, file descriptor, subscription status, and connection state. This setup facilitates the dynamic management of subscription requests, updating clients' subscription vectors as needed and ensuring that messages from UDP clients are forwarded only to subscribed TCP clients.

For the store-forward functionality, the initial approach of file-based message storage for each disconnected TCP client was deemed overly complex due to the continual need for file management operations. Instead, a simpler and more efficient method was implemented, using a hashtable where each key corresponds to the ID of a disconnected client and the value is a queue of messages pending for delivery upon the client's reconnection.

When the 'exit' command is issued, the system ensures that all connected TCP clients are properly closed before shutting down the server, securing a clean and orderly termination of the service.










