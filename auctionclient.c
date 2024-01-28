/* Auction Client Program - auctionclient.c
 * UQ CSSE2310 Assignment 4
 * Author: Muhammad Sulaman Khan
 * Student Number: 47511921
 */


// Includes
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <csse2310a3.h>
#include <csse2310a4.h>


// Defines
#define NUM_ARGS 2
#define PORT 1
#define HOSTNAME "localhost"
#define DEFAULT_PROTOCOL 0
#define USAGE_ERROR_CODE 2
#define CONNECTION_ERROR_CODE 1
#define SERVER_CLOSED_CODE 17
#define AUCTION_EXIT_CODE 9
#define COMMENT '#'
#define BLANK '\0'
#define SPACE ' '
#define MAX_FIELDS 2
#define BID ":bid\0"
#define OUTBID ":outbid"
#define SOLD ":sold"
#define UNSOLD ":unsold"
#define WON ":won"
#define LISTED ":listed"
#define QUIT "quit"
#define QUIT_CODE 0

/* ClientData Struct
 * ------------------------
 * A struct to hold all the information important for the client
 * 
 * port: the port number that the client is connecting to (taken from argv)
 * to: the file pointer to the socket that the client is writing to
 * from: the file pointer to the socket that the client is reading from
 * auctions: the number of auctions that client is currently involved in
 * lock: the lock that is used for mutual exclusion of the auctions variable
 */
typedef struct {
    const char* port;
    FILE* to;
    FILE* from;
    int* auctions;
    sem_t* lock;
} ClientData;

/* throw_usage_error()
 * ------------------------
 * Prints specified usage error message to stderr and exits with status 2
 */
void throw_usage_error(void) {
    fprintf(stderr, "Usage: auctionclient portnumber\n");
    exit(USAGE_ERROR_CODE);
}

/* throw_connection_error()
 * ------------------------
 * Prints specified connection error message to stderr and exits with status 1
 * 
 * port: the port number that the client was unable to connect to (taken from
 *       argv)
 */
void throw_connection_error(const char* port) {
    fprintf(stderr, "auctionclient: connection failed to port %s\n", port);
    exit(CONNECTION_ERROR_CODE);
}

/* server_closed()
 * ------------------------
 * Prints specified server closed message to stderr and exits with status 17
 */
void server_closed(void) {
    fprintf(stderr, "auctionclient: server closed connection\n");
    exit(SERVER_CLOSED_CODE);
}

/* exit_with_auctions()
 * ------------------------
 * Prints specified auctions exit message to stderr and exits with status 9
 */
void exit_with_auctions(void) {
    fprintf(stderr, "Quitting with auction(s) still in progress\n");
    exit(AUCTION_EXIT_CODE);
}

/* parse_command_line()
 * ------------------------
 * Parses the command line arguments, first validating the arguments and then 
 * returns the port number
 * 
 * argc: the number of arguments passed to the program
 * argv: the argument vector passed to the program
 * 
 * Returns: the port number that the client is connecting to (taken from argv)
 */
const char* parse_command_line(int argc, char** argv) {
    if (argc != NUM_ARGS) {  // Check number of command lines
        throw_usage_error();
    }

    return argv[PORT];  // Get and return port
}

/* attempt_connect()
 * ------------------------
 * Attempts to connect to the server, and if unsuccessful, throws a connection
 * error
 * 
 * port: the port number that the client is connecting to (taken from argv)
 * 
 * Returns: the socket file descriptor of the connection
 */
int attempt_connect(const char* port) {
    struct addrinfo* addressInfo = 0;
    struct addrinfo parameters;
    memset(&parameters, 0, sizeof(struct addrinfo));
    parameters.ai_family = AF_INET;  //  Set connection type to IPv4
    parameters.ai_socktype = SOCK_STREAM;  // Set sockettype to SOCK_STREAM

    if (getaddrinfo(HOSTNAME, port, &parameters, &addressInfo)) {
        freeaddrinfo(addressInfo);
        throw_connection_error(port);
    }

    int tempSocket = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
    if (connect(tempSocket, addressInfo->ai_addr, sizeof(struct sockaddr))) {
        freeaddrinfo(addressInfo);
        throw_connection_error(port);
    }

    freeaddrinfo(addressInfo);
    return tempSocket;
}

/* read_stdin()
 * ------------------------
 * Thread startup function that waits continuously for input from stdin, and 
 * handles the command by either sending it to the server or exiting the 
 * program
 * 
 * dataPtr: a casted void pointer to the ClientData struct that holds all the
 *          information important for the client
 * 
 * Returns: NULL
 */
void* read_stdin(void* dataPtr) {
    ClientData data = *(ClientData*)dataPtr;

    char* line;
    while ((line = read_line(stdin))) {
        if (line[0] == COMMENT || line[0] == BLANK) {
            continue;  // Skip line if current line is comment or blank
        } 

        if (!strcmp(line, QUIT)) {
            sem_wait(data.lock);  // Take lock
            if (*(data.auctions)) {
                printf("Auction in progress - can not exit yet\n");
                fflush(stdout);
            } else {
                exit(QUIT_CODE);
            }
            sem_post(data.lock);  // Release lock
        } else {
            fprintf(data.to, "%s\n", line);
            fflush(data.to);
        }

        free(line);
    }

    sem_wait(data.lock);  // Take lock
    if (*(data.auctions)) {
        free(data.auctions);
        exit_with_auctions();
    } else {
        free(data.auctions);
        exit(QUIT_CODE);
    }
    sem_post(data.lock);  // Release lock

    return NULL;  // Needed as no thread is joining on this
}

/* read_auctioneer()
 * ------------------------
 * Reads from the auctioneer and echos the lines to stdout, while handling the
 * required information to increment or decrement auctions variable
 * 
 * data: the ClientData struct that holds all the information important for the
 *       client
 */
void read_auctioneer(ClientData data) {
    char* line;
    char** response;

    while ((line = read_line(data.from))) {
        printf("%s\n", line);
        fflush(stdout);

        response = split_by_char(line, SPACE, MAX_FIELDS);

        if (!strcmp(response[0], BID) || !strcmp(response[0], LISTED)) {
            sem_wait(data.lock);
            (*(data.auctions))++;
            sem_post(data.lock);
        } else if (!strcmp(response[0], OUTBID) 
                || !strcmp(response[0], WON) 
                || !strcmp(response[0], SOLD) 
                || !strcmp(response[0], UNSOLD)) {
            sem_wait(data.lock);
            (*(data.auctions))--;
            sem_post(data.lock);
        }

        free(line);
        free(response);
    }

    server_closed();
}

/* main()
 * ------
 * Main function of program, spawns threads and runs all necessary functions in
 * order to connect to and read from the server
 *
 * argc: Number of command line arguments
 * argv: Array of command line arguments
 */
int main(int argc, char** argv) {
    ClientData data = {.port = parse_command_line(argc, argv), 
            .auctions = calloc(sizeof(int), 1)};

    sem_t lock;
    sem_init(&lock, 0, 1);
    data.lock = &lock;

    int tempSocket = attempt_connect(data.port);
    int tempSocketCopy = dup(tempSocket);

    data.to = fdopen(tempSocket, "w");
    data.from = fdopen(tempSocketCopy, "r");

    pthread_t threadId;
    pthread_create(&threadId, NULL, read_stdin, &data);
    pthread_detach(threadId);

    read_auctioneer(data);

    return 0;
}

