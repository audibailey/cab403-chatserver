//
// Created by Audi Bailey on 25/10/19.
//

#ifndef CAB403ASSESSMENT1_CLIENT_H
#define CAB403ASSESSMENT1_CLIENT_H
#pragma once

void sendMessageClient(int clientPos, char *message);
int noSub(int clientPos);
void channelSub(int channelNum, int clientPos);
void channelUnsub(int channelNum, int clientPos);

#endif //CAB403ASSESSMENT1_CLIENT_H
