//
// Created by Audi Bailey on 23/10/19.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include "constants.h"
#include "channel.h"

// Include Apple semaphore because I test on an Apple machine as well
#ifdef __APPLE__
#include <mach/semaphore.h>
#include <mach/task.h>
#define sem_post(a)         semaphore_signal(*((semaphore_t *)a))
#define sem_wait(a)         semaphore_wait(*((semaphore_t *)a))
#define sem_init(a,b,c)     semaphore_create(mach_task_self(), (semaphore_t *)a, SYNC_POLICY_FIFO, c)
#define sem_t               semaphore_t
#else
#include <semaphore.h>
#endif

/*
 * The function is in-case of a message overload, it pushes all the messages back so the top is always fresh
 *
 */
void channelPushLeft(int channelNum)
{
    // Loop through all the messages and shift them to the left
    for(int i = 0; i < BACKLOG-2; i++)
    {
        strcpy(storage->channels[channelNum][i].message, storage->channels[channelNum][i+1].message);
        storage->channels[channelNum][i].timePosted = storage->channels[channelNum][i+1].timePosted;
    }
}

/*
 * This is the channel "writer", it adds a message to the top of the channel (only one writer and cannot be read same time)
 */
void addToChannel(char *message, int channelNum) {
    // Wait for channel access in-case its blocked (being read or  written to)
    sem_wait(&storage->channelWriteBlock[channelNum]);
    // Find the position of the latest message
    int position = storage->backlogPos[channelNum];

    // Check for channel overflow
    if (position == BACKLOG-1) {
        // Delete the first message and shift all the messages back and put the new message on the top
        memset(storage->channels[channelNum][0].message, 0, MAX_MESSAGE);
        storage->channels[channelNum][0].timePosted = (time_t) (-1);
        channelPushLeft(channelNum);
        strcpy(storage->channels[channelNum][position].message, message);
        storage->channels[channelNum][position].timePosted = time(NULL);
    } else {
        // Put new message on top and increment where the top is
        strcpy(storage->channels[channelNum][position].message, message);
        storage->channels[channelNum][position].timePosted = time(NULL);
        storage->backlogPos[channelNum]++;
    }

    // Unblock the channel
    sem_post(&storage->channelWriteBlock[channelNum]);
}

/*
 * This is the channel "reader", it reads the channel message from the given position (allows multiple readers)
 */
char *readFromChannel(int channelNum, int position) {
    // Make a buffer for the message
    char *message = malloc(MAX_MESSAGE);

    // Wait for channel access in-case its blocked
    sem_wait(&storage->channelMutex[channelNum]);
    // Increment that someone is accessing the channel
    storage->channelReaderCount[channelNum] += 1;

    // Tell the writer it can't access it because someone is reading
    if (storage->channelReaderCount[channelNum] == 1) {
        sem_wait(&storage->channelWriteBlock[channelNum]);
    }
    // Unblock the channel in-case more readers want to access it
    sem_post(&storage->channelMutex[channelNum]);

    // Copy message to buffer
    strcpy(message, storage->channels[channelNum][position].message);

    // Wait for channel access in-case its blocked
    sem_wait(&storage->channelMutex[channelNum]);
    // Decrement that someone is access the channel
    storage->channelReaderCount[channelNum] -= 1;

    // If no one is actively reading, tell the write it can write
    if (storage->channelReaderCount[channelNum] == 0) {
        sem_post(&storage->channelWriteBlock[channelNum]);
    }
    // Unblock the channel
    sem_post(&storage->channelMutex[channelNum]);

    // Return the message
    return message;
}
