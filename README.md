# Chat Server

A game based off Tom and Jerry for the CAB202 unit at QUT. In this assignment, I have been commissioned to develop a client server system for an online multi-client message server. The purpose of this service is to manage a queue of text messages to be
delivered to connected/subscribed connections. All connected clients can add a message and
each message is associated with a channel ID. Clients are subscribed to multiple channels and
when any of the subscribed channels has a new message, the clients can fetch and see it.
The client and server will be implemented in the C programming language using BSD sockets
on the Linux operating system. This is the same environment used during the weekly practicals.
The programs (clients and server) are to run in a terminal reading input from the keyboard and
writing output to the screen.

## Issues with the code

None that I'm aware of.

## Installation

Clone it and go into it

`make`

## Usage

### Client 
`./client <ip> <port>`

### Server
`./server <port>`

### Commands

#### SUB <channelid>
Subscribe the client to the given channel ID to follow future messages sent to this channel. Channelid is an integer from 0-255. 

#### CHANNELS
Show a list of subscribed channels together with the statistics of how many messages have been
sent to that channel, how many messages are marked as read by this client, how many messages
are ready but have not yet been read by the client.

#### UNSUB <channelid>
Unsubscribe the client from the channel with the given channel ID.

#### NEXT <channelid>
Fetch and display the next unread message on the channel with the given ID.

#### LIVEFEED <channelid>
This command acts as a continuous version of the NEXT command, displaying all unread
messages on the given channel, then waiting, displaying new messages as they come in. Using
Ctrl+C to send a SIGINT signal to the client while in the middle of LIVEFEED will cause the
client to stop displaying the live feed and return to normal operation.

#### NEXT
This version of the NEXT command (without the channelid argument) will display the next
unread message out of all the channels the user is subscribed to.

#### LIVEFEED
This version of the LIVEFEED command displays messages continuously in the same way the
regular LIVEFEED command works, except it displays messages that appear on any channel
the user is subscribed to, like the parameterless NEXT command. Using
Ctrl+C to send a SIGINT signal to the client while in the middle of LIVEFEED will cause the
client to stop displaying the live feed and return to normal operation.

#### SEND <channelid> <message>
Send a new message to the channel with the given ID. Everything between the space after the
channel ID and before the line ending is the message that should be sent.

#### BYE
Closes the client’s connection to the server gracefully. This also effectively unsubscribes the
user from all channels on that server, so if the user reconnects, they will have to resubscribe.
The same thing should happen if the client is closed due to a SIGINT command being received
outside of LIVEFEED mode.

## Contributing

No contributions as this was a project for a university class that is now finished.

## License
[MIT](https://choosealicense.com/licenses/mit/)