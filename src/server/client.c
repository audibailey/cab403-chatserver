//
// Created by Audi Bailey on 25/10/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "constants.h"
#include "client.h"

/*
 * This function writes the message to the socket (sends the message to the client)
 */
void sendMessageClient(int clientPos, char *message) {
    // Ensure no one is accessing the socket and then send it
    pthread_mutex_lock(&storage->client[clientPos].localMutex);
    write(storage->client[clientPos].socket, message, strlen(message));
    pthread_mutex_unlock(&storage->client[clientPos].localMutex);
}

/*
 * This function checks to see if the client has subscribed to any channels
 */
int noSub(int clientPos) {
    // Loop through each channel and check if channel positions are greater than 0
    for (int i = 0; i < CHANNELS; i++) {
        if (storage->client[clientPos].channels[i] >= 0) {
            return 0;
        }
    }
    return 1;
}

/*
 * This function allows a client to subscribe to a channel
 */
void channelSub(int channelNum, int clientPos) {
    if (channelNum >= 0 && channelNum < CHANNELS) { // Check if the channel is within amount of available channels
        if (storage->client[clientPos].channels[channelNum] > 0) { // Check if the channel has already been subscribed to and tell the client
            char message[MAX_MESSAGE];
            sprintf(message, "Already subscribed to channel %i \n", channelNum);
            sendMessageClient(clientPos, message);
        } else { // Subscribe to the channel and tell the client
            storage->client[clientPos].channels[channelNum] = storage->backlogPos[channelNum];
            storage->client[clientPos].initChannel[channelNum] = storage->backlogPos[channelNum];
            char message[MAX_MESSAGE];
            sprintf(message, "Subscribed to channel %i \n", channelNum);
            sendMessageClient(clientPos, message);
        }
    } else { // Tell the client the channel is invalid
        char message[MAX_MESSAGE];
        sprintf(message, "Invalid channel: %i \n", channelNum);
        sendMessageClient(clientPos, message);
    }
}

/*
 * This function allows a client to unsubscribe from a channel
 */
void channelUnsub(int channelNum, int clientPos) {
    if (channelNum >= 0 && channelNum < CHANNELS) { // Check if the channel is within amount of available channels
        if (storage->client[clientPos].channels[channelNum] == -1) { // Check if the channel has already been unsubscribed to and tell the client
            char message[MAX_MESSAGE];
            sprintf(message, "Not subscribed to channel %i \n", channelNum);
            sendMessageClient(clientPos, message);
        } else { // Unsubscribe from the channel and tell the client
            storage->client[clientPos].channels[channelNum] = -1;
            storage->client[clientPos].initChannel[channelNum] = -1;
            char message[MAX_MESSAGE];
            sprintf(message, "Unsubscribed to channel %i \n", channelNum);
            sendMessageClient(clientPos, message);
        }
    } else { // Tell the client the channel is invalid
        char message[MAX_MESSAGE];
        sprintf(message, "Invalid channel: %i \n", channelNum);
        sendMessageClient(clientPos, message);
    }
}