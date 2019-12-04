cmake_minimum_required(VERSION 3.15)
project(CAB403Assessment1 C)

set(CMAKE_C_STANDARD 99)

#add_executable(CAB403Assessment1 server/main.c server/constants.h server/message.c server/message.h server/channel.c server/channel.h)
add_executable(src/server src/server/main.c src/server/constants.h src/server/message.c src/server/message.h src/server/client.c src/server/client.h src/server/channel.c src/server/channel.h)

add_executable(src/client src/client/main.c)