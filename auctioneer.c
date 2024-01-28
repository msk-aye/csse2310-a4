/* Auctioneer (Server) Program - auctioneer.c
 * UQ CSSE2310 Assignment 4
 * Author: Muhammad Sulaman Khan
 * Student Number: 4751192
 */


// Includes
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <csse2310a3.h>
#include <csse2310a4.h>


// Defines
#define MAX_ARGS 5
#define MAX_CLIENTS "--maxclients"
#define LISTEN_ON "--listenon"
#define DEFAULT_PORT "0"
#define USAGE_ERROR_CODE 17
#define PORT_ERROR_CODE 8
#define DEFAULT_PROTOCOL 0
#define CONNECTION_ERROR_CODE 69
#define PORT_MIN 1024
#define PORT_MAX 65535


/* ClientData Struct
 * ------------------------
 * A struct to hold all the information important client server communication
 *
 * id: Uniquely identifying id number of the client (same as the socket fd)
 * to: the file pointer to the socket that the client is writing to
 * from: the file pointer to the socket that the client is reading from
 * lock: the lock that is used for mutual exclusion of the auctions variable
 */
typedef struct {
    int id;
    FILE* to;
    FILE* from;
    sem_t* lock;
} ClientData;

/* Item Struct
 * ------------------------
 * A struct to hold all the information important for the auction items
 *
 * name: the name of the item
 * price: the current price of the item
 * expiry: the time at which the item will expire
 * winner: the client that has the highest bid on the item
*/
typedef struct {
    const char* name;
    int price;
    int expiry;
    ClientData winner;
} Item;

/* ServerData Struct
 * ------------------------
 * A struct to hold all the information important for the server
 *
 * maxClients: the maximum number of clients that can connect to the server
 * currClients: the current number of clients connected to the server
 * port: the port number that the server is listening on (taken from argv)
 * listenSocket: the socket that the server is listening on
 * items: the array of items that are being auctioned
 * lock: the lock that is used for mutual exclusion of the items array
 */
typedef struct {
    int maxClients;
    int currClients;
    const char* port;
    int listenSocket;
    Item* items;
    sem_t* lock;
} ServerData;

/* throw_usage_error()
 * ------------------------
 * Prints specified usage error message to stderr and exits with status 17
 */
void throw_usage_error(void) {
    fprintf(stderr, "Usage: auctioneer [--maxclients num-connections] "
            "[--listenon portnumber]\n");
    exit(USAGE_ERROR_CODE);
}

/* throw_port_error()
 * ------------------------
 * Prints specified port error message to stderr and exits with status 8
 */
void throw_port_error(void) {
    fprintf(stderr, "auctioneer: port can't be listened on\n");
    exit(PORT_ERROR_CODE);
}

/* throw_connection_error()
 * ------------------------
 * Prints specified connection error message to stderr and exits with status 69
 */
void throw_connection_error(void) {
    fprintf(stderr, "auctioneer: error accepting connection\n");
    exit(CONNECTION_ERROR_CODE);
}

/* check_digits()
 * ------------------------
 * Checks if all characters in a string are digits
 *
 * arg: the string to be checked
 *
 * returns: true if all characters are digits, false otherwise
*/
bool check_digits(char* arg) {
    for (int i = 0; i < strlen(arg); i++) {
        if (!isdigit(arg[i])) {
            return false;
        }
    }
    return true;
}

/* parse_command_line()
 * ------------------------
 * Parses the command line arguments and returns a populated ServerData struct
 *
 * argc: the number of arguments passed to the program
 * argv: the array of arguments passed to the program
 *
 * returns: a ServerData struct containing the parsed information
*/
ServerData parse_command_line(int argc, char** argv) {
    ServerData data = {.maxClients = 0, .port = 0};  // Initialise struct 
    bool portGiven = false;  // Variable to track if portnumber given

    if (argc > MAX_ARGS || !(argc % 2)) {
        throw_usage_error();
    }

    for (int i = 1; i < argc; i += 2) {
        if (!strcmp(argv[i], MAX_CLIENTS)) {
            if (!data.maxClients && check_digits(argv[i + 1])) {
                data.maxClients = atoi(argv[i + 1]);
                if (data.maxClients < 0) {
                    throw_usage_error();
                }
            } else {
                throw_usage_error();
            }

        } else if (!strcmp(argv[i], LISTEN_ON)) {
            if (!portGiven && check_digits(argv[i + 1])) {
                data.port = argv[i + 1];
                portGiven = true;
                if (strcmp(data.port, DEFAULT_PORT) && 
                        (atoi(data.port) < PORT_MIN || 
                        atoi(data.port) > PORT_MAX)) {
                    throw_usage_error();
                }
            } else {
                throw_usage_error();
            }

        } else {
            throw_usage_error();
        }
    }

    if (!portGiven) {
        data.port = DEFAULT_PORT;
    }

    return data;
}

/* open_port()
 * ------------------------
 * Opens a port for listening and returns the socket
 *
 * data: the ServerData struct containing the port number to be opened
 *
 * returns: the socket that is listening on the port
*/
int open_port(ServerData data) {
    struct addrinfo* addressInfo = 0;
    struct addrinfo parameters;
    memset(&parameters, 0, sizeof(struct addrinfo));
    parameters.ai_family = AF_INET;   // Set connection to IPv4
    parameters.ai_socktype = SOCK_STREAM;  // Set socket type to SOCK_STREAM
    parameters.ai_flags = AI_PASSIVE;    // Listen on all IP addresses

    if (getaddrinfo(NULL, data.port, &parameters, &addressInfo)) {
        freeaddrinfo(addressInfo);
        throw_port_error();
    }

    int listenSocket = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
    int optVal = 1;  // Allow address (port number) to be reused immediately
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, 
            &optVal, sizeof(int)) < 0) {
        freeaddrinfo(addressInfo);
        throw_port_error();
    }

    if (bind(listenSocket, addressInfo->ai_addr, 
            sizeof(struct sockaddr)) < 0) {
        freeaddrinfo(addressInfo);
        throw_port_error();
    }

    if (listen(listenSocket, data.maxClients) < 0) {  // Set socket to listen
        freeaddrinfo(addressInfo);
        throw_port_error();
    }

    struct sockaddr_in sockAddr;
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(listenSocket, (struct sockaddr*)&sockAddr, &len)) {
        freeaddrinfo(addressInfo);
        throw_port_error();
    } else {
        fprintf(stderr, "%d\n", ntohs(sockAddr.sin_port));
        fflush(stderr);
    }

    freeaddrinfo(addressInfo);
    return listenSocket;
}

/* client_thread()
 * ------------------------
 * The function that is run by each client thread, inclomplete
*/
void* client_thread(void* clientDataPtr) {
    // Wait for client commands
    // Ensure mutex
    // Once client disconnects, must close connection, cleanup and terminate.
    return NULL;
}

/* process_connections()
 * ------------------------
 * Accepts connections from clients and creates a thread for each client
 *
 * data: the ServerData struct containing the socket to listen on
*/
void process_connections(ServerData data) {
    int clientfd;
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(struct sockaddr_in);

    while (true) {  // Maybe while (!maxClients || currClients < maxClients)
        if ((clientfd = accept(data.listenSocket, 
                        (struct sockaddr*)&clientAddr, &len) < 0)) {
            throw_connection_error();
        }

        ClientData clientData; 
        int clientfdCopy = dup(clientfd);

        clientData.to = fdopen(clientfd, "w");
        clientData.from = fdopen(clientfdCopy, "r");
        
        pthread_t threadId;
        pthread_create(&threadId, NULL, client_thread, &clientData);
        pthread_detach(threadId);
    }
}

/* main()
 * ------------------------
 * Main function of program, parses command line, opens port and processes
 * connections
 *
 * argc: Number of command line arguments
 * argv: Array of command line arguments
 *
 * returns: 0 if program runs successfully
*/
int main(int argc, char** argv) {
    ServerData data = parse_command_line(argc, argv);
    data.listenSocket = open_port(data);
    process_connections(data);

    return 0;
}

