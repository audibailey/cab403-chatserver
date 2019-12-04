//
// Created by Audi Bailey on 21/10/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include "constants.h"
#include "client.h"
#include "channel.h"

static volatile bool localKeepRunning = true;

/*
 * This function is the signal handler for Ctrl+C it'll make sure that everything is cleanly exited
 */
void handleSIGINT(int signalType) {
    (void) signalType;
    localKeepRunning = false;
}

/*
 * This function checks all the channels and finds the latest message to send to the client ("NEXT" command)
 */
void readAllChannel(int clientPos) {
    // Set some default variables
    int messagePos = -1;
    int channelPos = -1;

    // Make sure the user is subbed to a channel
    if (noSub(clientPos)) {
        // Tell both the client and the server to stop LIVEFEED
        if(storage->client[clientPos].LIVESTREAM) {
            char *message = "KILL";
            sendMessageClient(clientPos, message);
            storage->client[clientPos].LIVESTREAM = false;
            sleep(1);
        }

        // Send the client that they aren't subscribed to any channels
        char *message = "Not subscribed to any channels.\n";
        sendMessageClient(clientPos, message);
    } else {
        // Loop through each channel
        for (int i = 0; i < CHANNELS; i++) {
            // Check if the client is subbed to the channel
            int messageMarker = storage->client[clientPos].channels[i];
            if (messageMarker >= 0 ) {
                // Check if the time the message is posted is not empty then set it as the new latest message
                if ((channelPos == -1 && messagePos == -1) && (storage->channels[i][messageMarker].timePosted != 0 && storage->channels[i][messageMarker].message[0] != 0)) {
                    channelPos = i;
                    messagePos = messageMarker;
                } else if (messagePos != -1 && channelPos != -1) {
                    // Check which has the latest message then set the variables to the latest message
                    if (storage->channels[i][messageMarker].timePosted < storage->channels[channelPos][messagePos].timePosted && storage->channels[i][messageMarker].message[0] != 0) {
                        channelPos = i;
                        messagePos = messageMarker;
                    }
                }
            }
        }

        // If a latest message has been found
        if (messagePos != -1 && channelPos != -1) {
            char message[MAX_MESSAGE];

            // Fetch the message from the channel and format it to send
            char *channelMessage = readFromChannel(channelPos, storage->client[clientPos].channels[channelPos]);
            sprintf(message, "%i:%s", channelPos, channelMessage);

            // Increment client message position in channel
            if (storage->backlogPos[channelPos] > storage->client[clientPos].channels[channelPos]) {
                storage->client[clientPos].channels[channelPos]++;
            }

            // Send message
            sendMessageClient(clientPos, message);
        }
    }
}

/*
 * This function is responsible for the LIVEFEED command
 */
void readAllChannelLoop(int clientPos) {
    // Loop until client ends LIVEFEED
    while(storage->client[clientPos].LIVESTREAM) {
        readAllChannel(clientPos);
        sleep(1);
    }
}

/*
 * This function sends "CHANNELS" response to the client
 */
void listChannelInfo(int clientPos) {
    // Loop through each channel
    for (int i = 0; i < CHANNELS; i++) {
        // Ensure they are subbed to the channel
        int messageMarker = storage->client[clientPos].channels[i];
        if (messageMarker >= 0) {
            // Get the values to send
            // This is calculated by getting the current position in the channel backlog
            int sinceStart = storage->backlogPos[i];
            // This is calculated by getting the position the user is upto and subtracting it from the position the messages were up to when the user subbed
            int sinceSubbed = (storage->client[clientPos].channels[i] - storage->client[clientPos].initChannel[i]);
            // This is calculated by getting the backlog position and subtracting it from where the user is up to
            int notRead = (storage->backlogPos[i] - storage->client[clientPos].channels[i]);

            // Format the response then send it
            char message[MAX_MESSAGE];
            sprintf(message, "%i\t%i\t%i\t%i\n", i, sinceStart, sinceSubbed, notRead);
            sendMessageClient(clientPos, message);
        }
    }
}

/*
 * This function finds the thread that the client has open, running a different command
 */
int findMyPID(pid_t thread, int clientPos) {
    // Loop through each thread
    for (int i = 0; i < storage->client[clientPos].clientCount; i++) {
        // If the thread matches return where it is in the list of client threads
        if (storage->client[clientPos].clientThreads[i].thread == thread) {
            return i;
        }
    }

    return -1;
}

/*
 * This function is responsible for storing the message in the channel buffer
 */
void sendMessage(char *message, int channelNum, int clientPos) {
    // Check if the channel is within amount of available channels
    if (channelNum >= 0 && channelNum < CHANNELS) {
        // Clean up the message (remove the SEND and channel number)
        char messageDupe[MAX_MESSAGE] = {0};
        strcpy(messageDupe, message);
        char *cleanedMessage;
        cleanedMessage = strchr(messageDupe, ' ');
        cleanedMessage++;
        cleanedMessage = strchr(cleanedMessage, ' ');
        cleanedMessage++;

        // Add to the channel
        addToChannel(cleanedMessage, channelNum);
    } else {
        // Send the client that channel is invalid
        char errMessage[MAX_MESSAGE];
        sprintf(errMessage, "Invalid channel: %i \n", channelNum);
        sendMessageClient(clientPos, errMessage);
    }

}

/*
 *  This function is responsible for reading the latest message the user is up to in a particular channel (NEXT <channel>)
 */
void readMessage(int channelNum, int clientPos)
{
    if (channelNum >= 0 && channelNum <= CHANNELS-1){ // Check if the channel is within amount of available channels
        if (storage->client[clientPos].channels[channelNum] == -1) { // Check if the client is subbed to the channel and tell them
            char message[MAX_MESSAGE];
            sprintf(message, "Not subscribed to channel %i \n", channelNum);
            sendMessageClient(clientPos, message);
        } else { // Send the message in the channel and update what message the user is up to
            char *message = readFromChannel(channelNum, storage->client[clientPos].channels[channelNum]);
            if (storage->backlogPos[channelNum] > storage->client[clientPos].channels[channelNum]) {
                storage->client[clientPos].channels[channelNum]++;
            }
            sendMessageClient(clientPos, message);
        }
    } else {
        // Tell both the client and the server to stop LIVEFEED
        if(storage->client[clientPos].LIVESTREAM) {
            char *message = "KILL";
            sendMessageClient(clientPos, message);
            storage->client[clientPos].LIVESTREAM = false;
            sleep(1);
        }

        // Send the client that they aren't subscribed to any channels
        char message[MAX_MESSAGE];
        sprintf(message, "Invalid channel: %i \n", channelNum);
        sendMessageClient(clientPos, message);
    }
}

/*
 * This function is responsible for the LIVEFEED <channel> command
 */
void readMessageLoop(int channelNum, int clientPos) {
    // Loop until client ends LIVEFEED
    while(storage->client[clientPos].LIVESTREAM) {
        readMessage(channelNum, clientPos);
        sleep(1);
    }
}

/*
 * This function is responsible for parsing the message that was sent by the client
 */
void executePayload(int clientPos) {
    // Set some buffers for the messages
    char command[BUFFER_MAX];
    char argument[BUFFER_MAX];
    char message[MAX_MESSAGE];

    // Set some places to store values
    int arguments = storage->client[clientPos].args.argCount;
    strcpy(command, storage->client[clientPos].args.command);
    strcpy(argument, storage->client[clientPos].args.argument);
    strcpy(message, storage->client[clientPos].args.message);

    // Find the thread location in clients list of threads
    pid_t pidID = getpid();
    int threadPos = findMyPID(pidID, clientPos);

    // Split into how many arguments were sent
    if (arguments > 2) {
        // SEND command
        if (strcmp(command, "SEND") == 0) {
            int numChannel = atoi(argument);
            sendMessage(message, numChannel, clientPos);
        } else {
            char *errorMessage = "Invalid Command \n";
            sendMessageClient(clientPos, errorMessage);
        }
    } else if (arguments == 2) {
        if (strcmp(command, "SUB") == 0) { // SUB command
            int numChannel = atoi(argument);
            channelSub(numChannel, clientPos);
        } else if (strcmp(command, "UNSUB") == 0) { // UNSUB command
            int numChannel = atoi(argument);
            channelUnsub(numChannel, clientPos);
        } else if (strcmp(command, "NEXT") == 0) { // NEXT command
            int numChannel = atoi(argument);
            // Update thread specific command (to use later to kill it)
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, command);
            readMessage(numChannel, clientPos);
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, "");
        } else if (strcmp(command, "LIVEFEED") == 0) { // LIVEFEED command
            int numChannel = atoi(argument);
            // Update thread specific command (to use later to kill it) and update its currently in LIVEFEED mode
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, command);
            storage->client[clientPos].LIVESTREAM = true;
            readMessageLoop(numChannel, clientPos);
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, "");
        } else {
            char *message = "Invalid Command \n";
            sendMessageClient(clientPos, message);
        }
    } else {
        if (strcmp(command, "CHANNELS") == 0) { // CHANNELS command
            listChannelInfo(clientPos);
        } else if (strcmp(command, "NEXT") == 0) { // NEXT command
            // Update thread specific command (to use later to kill it)
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, command);
            readAllChannel(clientPos);
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, "");
        } else if (strcmp(command, "LIVEFEED") == 0) { // LIVEFEED command
            // Update thread specific command (to use later to kill it) and update its currently in LIVEFEED mode
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, command);
            storage->client[clientPos].LIVESTREAM = true;
            readAllChannelLoop(clientPos);
            strcpy(storage->client[clientPos].clientThreads[threadPos].command, "");
        } else if (strcmp(command, "STOP") == 0) { // STOP command
            // Loop through each of the client open threads
            for (int i = 0; i < storage->client[clientPos].clientCount; i++) {
                // Find any thread with a pending NEXT command and kill it or a LIVEFEED command and end its LIVEFEED mode
                if (strcmp(storage->client[clientPos].clientThreads[i].command, "NEXT") == 0) {
                    kill(storage->client[clientPos].clientThreads[i].thread, SIGINT);
                } else if (strcmp(storage->client[clientPos].clientThreads[i].command, "LIVEFEED") == 0) {
                    storage->client[clientPos].LIVESTREAM = false;
                }
            }

        } else if (strcmp(command, "BYE") == 0) { // BYE command, disconnect the client
            storage->client[clientPos].keepAlive = false;
        } else {
            char *invalidMessage = "Invalid Command \n";
            sendMessageClient(clientPos, invalidMessage);
        }
    }

    // Self thread clean up
    storage->client[clientPos].clientCount--;
}

/*
 * This is the entry point function when the client is connected
 */
void acceptPayload(int clientPos) {
    // Set a buffer
    char message[MAX_MESSAGE] = {0};

    // Setup SIGINT catch to cleanly exit
    signal(SIGINT, handleSIGINT);
    // Loop until the client is terminated/closed
    while (storage->client[clientPos].keepAlive && localKeepRunning) {
        // Initialize file descriptors and zero them
        fd_set readFD, writeFD, exceptFD;
        FD_ZERO(&readFD);
        FD_ZERO(&writeFD);
        FD_ZERO(&exceptFD);

        // Set read FD to accept from the client connection socket
        FD_SET(storage->client[clientPos].socket, &readFD);

        // Set timeout to 1 second
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Wait for client socket buffer to become ready
        if (select(storage->client[clientPos].socket + 1, &readFD, &writeFD, &exceptFD, &timeout) >= 1) {
            // Read from the socket safely
            pthread_mutex_lock(&storage->client[clientPos].localMutex);
            storage->client[clientPos].readSize = read(storage->client[clientPos].socket, message, MAX_MESSAGE);
            pthread_mutex_unlock(&storage->client[clientPos].localMutex);

            // If the socket had anything in it
            if (storage->client[clientPos].readSize > 0) {

                // Define some buffers
                char command[BUFFER_MAX] = {0};
                char argument[BUFFER_MAX] = {0};
                char messageDump[MAX_MESSAGE] = {0};

                // Count the amount of arguments
                int splitArgs = sscanf(message, " %s %s %s ", command, argument, messageDump);

                /*
                 * For debugging only
                 */

                /* printf("ARGUMENT COUNT: %d. \n", splitArgs);
                printf("COMMAND STRING: %s. \n", command);
                printf("ARGUMENTS: %s, %s \n", argument, messageDump); */

                // Assign the client message to some variables to be sent to a new thread
                storage->client[clientPos].args.argCount = splitArgs;
                strcpy(storage->client[clientPos].args.command, command);
                strcpy(storage->client[clientPos].args.argument, argument);
                strcpy(storage->client[clientPos].args.message, message);

                pid_t clientCommandPID = fork();
                if (clientCommandPID == -1) {
                    perror("Failed to fork client on socket %d.\n");
                    continue;
                } else if (clientCommandPID == 0) {
                    storage->client[clientPos].clientThreads[storage->client[clientPos].clientCount].thread = getpid();
                    storage->client[clientPos].clientCount++;
                    executePayload(clientPos);

                    exit(EXIT_SUCCESS);
                } else {
                    // Clear buffers
                    memset(message, 0, MAX_MESSAGE);
                    memset(messageDump, 0, BUFFER_MAX);
                    memset(command, 0, BUFFER_MAX);
                    memset(argument, 0, BUFFER_MAX);
                    memset(messageDump, 0, BUFFER_MAX);
                }
            } else if (storage->client[clientPos].readSize == 0) {
                storage->client[clientPos].keepAlive = false;
            }
        }
        // Infinite loop and select() causes high CPU so sleep for 1 second to ensure that doesnt happen
        sleep(1);
    }

    // Reap grandchildren
    for (int i = 0; i < storage->client[clientPos].clientCount; i++) {
        kill(storage->client[clientPos].clientThreads[storage->client[clientPos].clientCount].thread, SIGKILL);
    }

    // When client is terminated: close the socket, mark the client space as free and self thread clean up
    shutdown(storage->client[clientPos].socket, SHUT_WR);
    close(storage->client[clientPos].socket);
    storage->client[clientPos].free = true;
}

