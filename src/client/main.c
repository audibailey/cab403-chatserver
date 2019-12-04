//
// Created by Audi Bailey on 20/9/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <signal.h>

// Set the max message buffer from the task sheet
#define MAX_MESSAGE 1024

// Setup some globals. The 2 threads, the 2 edge cases and the socket
pthread_t commandThread, serverThread;
static volatile int keepAlive = 1;
static volatile int liveStream = 0;
int socketID;

/*
 * The function that handles the SIGINT
 */
void handleSIGINT(int signalType)
{
    // Ensure it was a SIGINT
    if (signalType == SIGINT) {
        // Check if its running in LIVEFEED Mode
        if (liveStream) {
            // Kill LIVEFEED mode and tell the server its no longer in LIVEFEED mode
            liveStream = 0;
            send(socketID, "STOP", strlen("STOP"), 0);
        } else {
            // Tell the server the client wants to disconnect
            send(socketID, "BYE", strlen("BYE"), 0);
        }
    }
}

/*
 * This is function, as a thread, is in charge of accepting commands to send to the server
 */
void *commands(void)
{
    // Register the signal handler for Ctrl+C
    struct sigaction signalAction;
    signalAction.sa_flags = 0;
    signalAction.sa_handler = handleSIGINT;
    sigemptyset(&signalAction.sa_mask);
    sigfillset(&signalAction.sa_mask);
    if (sigaction(SIGINT, &signalAction, NULL) < 0) {
        perror("Error handling SIGINT.\n");
    }

    // Loop until ending the connection
    while (keepAlive) {
        // Initialize file descriptors and zero them
        fd_set readFD, writeFD, exceptFD;
        FD_ZERO(&readFD);
        FD_ZERO(&writeFD);
        FD_ZERO(&exceptFD);

        // Set read FD to accept from STDIN (command input)
        FD_SET(STDIN_FILENO, &readFD);

        // Set timeout to 1.5 seconds
        struct timeval selectTimeout;
        selectTimeout.tv_sec = 1;
        selectTimeout.tv_usec = 500000;

        // Define some defaults for storing command information
        char userInput[MAX_MESSAGE] = {0};
        char command[120] = {0};
        size_t userInputSize = MAX_MESSAGE;

        // Wait for input to become ready
        if (select(STDIN_FILENO+1, &readFD, &writeFD, &exceptFD, &selectTimeout) == 1) {
            // Store STDIN in userInput
            fgets(userInput, userInputSize, stdin);

            // Parse the command and store it in command
            sscanf(userInput, "%s ", command);

            // Send the command to the server
            write(socketID, userInput, strlen(userInput));

            // If the command was LIVEFEED or STOP change the respected thing
            if (strcmp(command, "LIVEFEED") == 0) {
                liveStream = 1;
            } else if (strcmp(command, "STOP") == 0) {
                liveStream = 0;
            }
        }
    }

    // Return Success
    return EXIT_SUCCESS;
}

/*
 * This function, as a thread, is in charge of receiving server responses
 */
void *serverResponse(void) {
    // Register the signal handler for Ctrl+C
    struct sigaction signalAction;
    signalAction.sa_flags = 0;
    signalAction.sa_handler = handleSIGINT;
    sigemptyset(&signalAction.sa_mask);
    sigfillset(&signalAction.sa_mask);
    if (sigaction(SIGINT, &signalAction, NULL) < 0) {
        perror("Error handling SIGINT.\n");
    }

    // Loop until ending the connection
    while (keepAlive)
    {
        // Initialize file descriptors and zero them
        fd_set readFD, writeFD, exceptFD;
        FD_ZERO(&readFD);
        FD_ZERO(&writeFD);
        FD_ZERO(&exceptFD);

        // Set read FD to accept from the server connection socket
        FD_SET(socketID, &readFD);

        // Set timeout to 1.5 seconds
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 500000;

        // Define the location for storing the servers response
        char serverResponse[MAX_MESSAGE] = {0};

        // Wait for server socket buffer to become ready
        if (select(socketID+1, &readFD, &writeFD, &exceptFD, &timeout) >= 1)
        {
            // Store server response from socket in serverResponse and save status of response to num
            size_t connectionStatus = read(socketID, serverResponse, MAX_MESSAGE);

            // Check if the connection is open still, check that the response isn't empty
            if(connectionStatus > 0 && strcmp(serverResponse, "") != 0) {
                // If the server response with KILL it means LIVEFEED mode should be disabled
                if (strcmp("KILL", serverResponse) == 0) {
                    liveStream = 0;
                }

                // If the server doesnt respond with KILL, print the server's response
                if (strcmp("KILL", serverResponse) != 0){
                    printf("%s", serverResponse);
                }
                memset(serverResponse, 0, sizeof(serverResponse));
            } else if (connectionStatus == 0) { // Check if connection is closed
                // Tell the client connection is closed and kill the client
                printf("Connection Closed");
                keepAlive = 0;
            }
        }
    }

    // Return Success
    return EXIT_SUCCESS;
}

/*
 * This is the entry point to the client program
 */
int main(int argc, char const *argv[])
{
    // Define server connection variable and socket
    struct sockaddr_in serverConnection;

    // Initialise the socket into the variable socketID
    if ((socketID = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error \n");
        return EXIT_FAILURE;
    }

    // Get port from arguments or set as default
    int port = argc > 2 ? atoi(argv[2]) : 6789;

    // Setup server connection
    serverConnection.sin_family = AF_INET;
    serverConnection.sin_port = htons(port);

    // Bind socket to serverConnection
    if(inet_pton(AF_INET, argv[1], &serverConnection.sin_addr) <= 0)
    {
        printf("Invalid address\n");
        return EXIT_FAILURE;
    }

    // Connect to the server
    if (connect(socketID, (struct sockaddr *)&serverConnection, sizeof(serverConnection)) < 0)
    {
        printf("Connection Failed \n");
        return EXIT_FAILURE;
    }

    // Create the Command Thread and the Server Response Thread
    pthread_create(&commandThread, NULL, (void *(*)(void *)) commands, NULL);
    pthread_create(&serverThread, NULL, (void *(*)(void *)) serverResponse, NULL);

    // Synchronise Threads
    pthread_join(commandThread, NULL);
    pthread_join(serverThread, NULL);

    // Return Success
    return EXIT_SUCCESS;
}
