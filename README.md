# Chat Server and Client
Created by Andrew Jack and Kevin Wang

To make the project, run "make"
To run the server, run "./server 'port'"
To run the client, run "./client 'hostname' 'port' 'username'"
'port' is your IP address

Client Functionality
- type into terminal to chat
- shows timestamp and sender of messages
- type "#users" to display currently connected users

Server Functionality
- real-time chat for multiple clients
- keeps track of connected users
- disconnects users inactive for 10 minutes
- disconnects user if name already in use
- log is generated once the server is terminated
- log file keeps track of users messages and when they entered and exited
the conversation

Server Architecture
The server uses sockets to connect to clients, and select to
perform non-blocking I/O. In the main loop, the server checks
for any input from clients, any new connection requests, and
any client timeouts. Only one process is used in order to preserve
the simplicity of the architecture.
