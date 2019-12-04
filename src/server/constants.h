//
// Created by Audi Bailey on 21/10/19.
//

#ifndef CAB403ASSESSMENT1_CONSTANTS_H
#define CAB403ASSESSMENT1_CONSTANTS_H
#pragma once

// Include Apple semaphore because I test on an Apple machine as well
#ifdef __APPLE__
#include <mach/semaphore.h>
#include <mach/task.h>
#define sem_t               semaphore_t
#else
#include <semaphore.h>
#endif

#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

// Define some global constants
#define MAXCONNECTIONS 10 // Amount of clients that can connect at once
#define MAX_MESSAGE 1024 // Max message length
#define CHANNELS 256 // Amount of available channels
#define BACKLOG 1000 // Amount of available messages per channel
#define BUFFER_MAX 120 // Max buffer length
#define CLIENT_THREADS 10 // Max amount of threads a single client can use for executing commands

// This struct stores the message data
typedef struct messages message_t;
struct messages
{
    char message[MAX_MESSAGE];
    time_t timePosted;
};

// This struct stores the clients executing commands
typedef struct threadManage threadManage_t;
struct threadManage
{
    pid_t thread; // The thread the command is on
    char command[BUFFER_MAX]; // The command
};

// This struct stores the command and message details of a client
typedef struct executeArgs args_t;
struct executeArgs
{
    int argCount; // How many arguments where sent
    char command[BUFFER_MAX]; // What the initial command was (SEND, NEXT, SUB etc)
    char argument[BUFFER_MAX]; // The channel
    char message[MAX_MESSAGE]; // The message if it was a SEND command
};

// This struct stores everything in relation to the client
typedef struct client client_t;
struct client {
    char name[70]; // The client name (default as the IP)
    int socket; // The clients socket
    int channels[CHANNELS]; // The clients channel sub status/position (-1 = unsub, 0 >= message position)
    int initChannel[CHANNELS]; // The clients channel sub position when they first sub
    bool LIVESTREAM; // The client status if they are running a LIVEFEED command
    pthread_mutex_t localMutex; // Locking the socket so no overwriting/reading occurs
    bool keepAlive; // The clients connection situation (false = close connection)
    size_t readSize; // The clients buffer size from reading the socket
    threadManage_t clientThreads[CLIENT_THREADS]; // The clients executing threads commands
    int clientCount; // How many threads the client is using on commands
    args_t args; // The message details of the client
    bool free; // Whether the client data is available to be reassigned
};

typedef struct sharedMemory sharedMem_t;
struct sharedMemory
{
    int backlogPos[CHANNELS]; // The counter that keeps track of where the top stack of a channel is
    int channelReaderCount[CHANNELS]; // The counter that keeps track of concurrent reading of a single channel
    sem_t channelMutex[CHANNELS]; // The reading semaphore array, ensures no one can read while writing.
    sem_t channelWriteBlock[CHANNELS]; // The writing semaphore array, ensures when the channel is available to write to
    message_t channels[CHANNELS][BACKLOG]; // The 2D array thats stores all the messages
    client_t client[MAXCONNECTIONS]; // The client array, keeps track of each client and their data
};

// Make shared memory a global
sharedMem_t *storage;

#endif //CAB403ASSESSMENT1_CONSTANTS_H
