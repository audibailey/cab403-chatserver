//
// Created by Audi Bailey on 20/9/19.
//
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <arpa/inet.h>

// Include Apple semaphore because I test on an Apple machine as well
#ifdef __APPLE__
#include <mach/semaphore.h>
#include <mach/task.h>
#define sem_init(a,b,c)     semaphore_create(mach_task_self(), (semaphore_t *)a, SYNC_POLICY_FIFO, c)
#define sem_t               semaphore_t
#else
#include <semaphore.h>
#endif

#include "constants.h"
#include "message.h"
#include "client.h"

// Define some globals
static volatile bool keepRunning = true;
static volatile bool SIGTERMED = false;

/*
 * This function is the signal handler for Ctrl+C it'll make sure that everything is cleanly exited
 */
void handleSigInt(int signalType) {

    // Break the main server loop to begin the graceful server exit
    (void) signalType;
    SIGTERMED = true;
    keepRunning = false;
}

/*
 * This initiates the clients, the semaphores and the channel statuses.
 */
void initSharedMemoryData(void) {
    for (int i = 0; i < MAXCONNECTIONS; i++) {
        storage->client[i].free = true;
    }
    for (int i = 0; i < CHANNELS; i++) {
        sem_init(&storage->channelMutex[i],0,1);
        sem_init(&storage->channelWriteBlock[i],0,1);
    }
    for (int i = 0; i < CHANNELS; i++){
        storage->backlogPos[i] = 0;
        storage->channelReaderCount[i] = 0;
    }
}

/*
 * This checks if there is room for a client to be processed
 */
int freeArraySlot(int clientSocket, struct sockaddr_in clientAddress) {
    int clientNo = -1;
    while (clientNo == -1) {
        // Loop through all the clients and find one that's free
        printf("Attempting to give %s connecting on socket %d, a position.\n", inet_ntoa(clientAddress.sin_addr), clientSocket);
        for (int i = 0; i < MAXCONNECTIONS; i++) {
            if (storage->client[i].free) {
                clientNo = i;
            }
        }
        // Tell the client that the server is full
        if (clientNo == -1) {
            char *message = "Server is full, please wait. Retrying to connect again in 10 seconds.\n";
            send(clientSocket, message, strlen(message), 0);
            sleep(10);
        }
    }
    return clientNo;
}

/*
 * This is the entry point to the server program
 */
int main(int argc, const char *argv[]) {
    // Initialise some variables
    int masterSocket;
    struct sockaddr_in serverAddress;
    socklen_t serverAddressSize = sizeof(serverAddress);

    // Get port from arguments or set as default
    int port = argc > 1 ? atoi(argv[1]) : 6789;
    if (!port) {
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Setup server address details
    memset(&serverAddress, 0, serverAddressSize);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Initialise the socket into the variable masterSocket
    if ((masterSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Failed to initiate socket. \n");
        exit(EXIT_FAILURE);
    }

    // Bind socket to serverAddress
    if (bind(masterSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        printf("Failed to bind socket. \n");
        exit(EXIT_FAILURE);
    }

    // Listen on socket to start receiving clients, with a max of MAX CONNECTIONS
    if (listen(masterSocket, MAXCONNECTIONS) == -1) {
        printf("Failed to listen on socket. \n");
        exit(EXIT_FAILURE);
    }

    // Configure the shared memory
    storage = mmap(NULL, sizeof(sharedMem_t), PROT_WRITE | PROT_READ, MAP_SHARED| MAP_ANONYMOUS, -1, 0);
    if (storage == MAP_FAILED) {
        perror("Unable to configure shared memory.\n");
        exit(EXIT_FAILURE);
    }
    initSharedMemoryData();

    // Print some status information
    printf("Server started at %d \n", port);
    printf("Ctrl+C to stop the server. \n");

    // Ensure my signal is not blocked by the accept command
    struct sigaction signalAction;
    signalAction.sa_flags = 0;
    signalAction.sa_handler = handleSigInt;
    sigemptyset(&signalAction.sa_mask);
    sigfillset(&signalAction.sa_mask);
    if (sigaction(SIGINT, &signalAction, NULL) < 0) {
        perror("Error handling SIGINT");
    }

    while (keepRunning) {
        // Initialise some client connection variables
        struct sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);
        memset(&clientAddress, 0, clientAddressSize);

        // Check for incoming clients
        int clientSocket = accept(masterSocket, (struct sockaddr *) &clientAddress, &clientAddressSize);
        if (clientSocket == -1) {
            if (!SIGTERMED) {
                printf("Failed to accept client on socket %d.\n", clientSocket);
            }
            continue;
        } else {
            // Create a thread just for the client
            pid_t clientPID = fork();
            if(clientPID == -1) {
                perror("Failed to fork client on socket %d. ");
                continue;
            } else if (clientPID == 0) {
                // Configuring the clients information
                int clientNo = freeArraySlot(clientSocket, clientAddress);
                storage->client[clientNo].socket = clientSocket;
                for (int i = 0; i < CHANNELS; i++) {
                    storage->client[clientNo].channels[i] = -1;
                }
                storage->client[clientNo].LIVESTREAM = false;
                storage->client[clientNo].keepAlive = true;
                storage->client[clientNo].readSize = 0;
                storage->client[clientNo].clientCount = 0;
                storage->client[clientNo].args.argCount = 0;
                storage->client[clientNo].free = false;
                strcpy(storage->client[clientNo].name, inet_ntoa(clientAddress.sin_addr));

                // Send welcome message
                char welcomeMessage[MAX_MESSAGE];
                sprintf(welcomeMessage, "Welcome your client ID is: %d.\n", clientNo);
                sendMessageClient(clientNo, welcomeMessage);

                // Start accepting the clients payloads
                acceptPayload(clientNo);

                printf("Disconnecting client %s on socket %d. \n", inet_ntoa(clientAddress.sin_addr), clientSocket);
                // Kill the fork now its work is complete
                exit(EXIT_SUCCESS);
            } else {
                // Client has been handed off already we can safely close the connection from the parent
                close(clientSocket);
            }
        }
    }

    // Reap Running Clients
    for (int i = 0; i < MAXCONNECTIONS; i++) {
        storage->client[i].keepAlive = false;
    }

    // Unmap shared memory
    int unmap_result = munmap(storage, sizeof(sharedMem_t));
    if (unmap_result == -1) {
        perror("Failed to unmap shared memory.\n");
        exit(EXIT_FAILURE);
    }

    // Return Success
    return EXIT_SUCCESS;
}