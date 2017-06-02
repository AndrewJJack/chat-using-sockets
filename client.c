#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

#define STDIN 0 // file descriptor for standard input
#define MAX_USERS 128

char *connected_users[MAX_USERS];

void welcome(char *username, char *server, int port) {
  printf("Welcome to the chat, %s!\n", username);
  printf("Attemping to connect to server %s at port %d\n", server, port);
}

/*
Estables connection to the server and returns a socket descriptor (int)
*/
int connectToServer(char *hostname, int port) {
  int s;
  struct sockaddr_in server;
  struct hostent *host;
  host = gethostbyname(hostname);
  if (host == NULL) {
    perror("Client: cannot get host description");
    exit(1);
  }
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("Client: cannot open socket");
    exit(1);
  }
  bzero(&server, sizeof(server));
  bcopy(host->h_addr, &(server.sin_addr), host->h_length);
  server.sin_family = host->h_addrtype;
  server.sin_port = htons(port);
  if (connect(s, (struct sockaddr *)&server, sizeof(server))) {
    perror("Client: cannot connect to server");
    exit(1);
  }
  return s;
}

/*
* Returns the current time as a string.
*/
char *timeString;
char *getTime() {
  time_t mytime;
  mytime = time(NULL);
  timeString = ctime(&mytime);
  timeString[strlen(timeString) - 1] = '\0';
  return timeString;
}

void printConnectedUsers() {
  int i;
  int userCount = 0;
  for (i = 0; i < MAX_USERS; i++) {
    if (connected_users[i] != NULL) {
      printf("%d. %s\n", ++userCount, connected_users[i]);
    }
  }
}

void performHandshake(int s, char *username) {
  // check that two bytes 0xCF 0xA7 are sent from the server
  uint8_t byte1;
  uint8_t byte2;
  read(s, &byte1, sizeof(byte1));
  read(s, &byte2, sizeof(byte2));
  if (byte1 != 0xCF || byte2 != 0xA7) {
    printf("Handshake failed!\n");
    exit(1);
  }
  // receive user list from the server
  uint16_t num_users;
  read(s, &num_users, sizeof(num_users));
  num_users = ntohs(num_users);
  if (num_users == 0) {
    printf("No other users!\n");
  } else {
    printf("Number of other users in the chat room: %d\n", num_users);
  }
  // store the user list
  int i;
  for (i = 0; i < num_users; i++) {
    uint8_t len;
    read(s, &len, sizeof(len));
    char *name = (char *)malloc((len + 1) * sizeof(char)); // store username string on the heap
    read(s, name, len);
    name[len] = '\0';
    connected_users[i] = name;
  }
  connected_users[i] = username;
  printf("All users in chat room:\n");
  printConnectedUsers();

  // send server the username
  uint8_t username_len = strlen(username);
  write(s, &username_len, sizeof(username_len));
  write(s, username, username_len);
  printf("Handshake success! Beginning chat...\n");
}

void sendMessage(int s, char *message) {
  if (message == NULL) { // send keepalive
    uint16_t keepalive = 0;
    write(s, &keepalive, sizeof(keepalive));
  } else {
    uint16_t len = strlen(message);
    uint16_t len_n = htons(len);
    write(s, &len_n, sizeof(len_n));
    write(s, message, len);
  }
}

void userJoin(char *username) {
  printf("[%s] User %s has joined!\n", getTime(), username);
  int i;
  for (i = 0; i < MAX_USERS; i++) {
    if (connected_users[i] == NULL) {
      connected_users[i] = username;
      break;
    }
  }
  printf("Total users:\n");
  printConnectedUsers();
}

void userLeave(char *username) {
  printf("[%s] User %s has disconnected!\n", getTime(), username);
  int i = 0;
  for (i = 0; i < MAX_USERS; i++) {
    if (connected_users[i] != NULL &&
        strcmp(username, connected_users[i]) == 0) {
      free(connected_users[i]);
      connected_users[i] = NULL;
    }
  }
}

void handleIncomingMessage(int s) {
  // get message type
  uint8_t msg_type;
  int nbytes = read(s, &msg_type, sizeof(msg_type));
  if (nbytes == 0) {
    printf("Server has terminated your connection. Exiting...\n");
    exit(1);
  }

  // get username
  uint8_t username_len;
  read(s, &username_len, sizeof(username_len));
  char *username = (char *)malloc((username_len + 1) * sizeof(char));
  read(s, username, username_len);
  username[username_len] = '\0';

  if (msg_type == 0x00) { // chat message
    // get message
    uint16_t msg_len;
    read(s, &msg_len, sizeof(msg_len));
    msg_len = ntohs(msg_len);
    char msg[msg_len + 1];
    read(s, &msg, msg_len);
    msg[msg_len] = '\0';
    // print incoming message
    printf("[%s] %s: %s\n", getTime(), username, msg);
  } else if (msg_type == 0x01) { // user join
    userJoin(username);
  } else if (msg_type == 0x02) { // user leave
    userLeave(username);
  }
}

int main(int argc, char *argv[]) {

  printf("------------------------------------------\n");
  printf("WECOME TO KEVIN AND ANDREW'S CHAT PROGRAM!\n");
  printf("------------------------------------------\n");

  char *hostname;
  int port;
  char *username;

  if (argc != 4) {
    printf("Not enough arguments supplied to chat client! Using defaults.\n");
    username = "Kevin";
    hostname = "localhost";
    port = 6969;
  } else {
    hostname = argv[1];
    port = atoi(argv[2]);
    username = argv[3];
    printf("Connecting to host %s at port %d with username %s\n", hostname, port, username);
  }

  welcome(username, hostname, port);
  int s = connectToServer(hostname, port);
  performHandshake(s, username);
  printf("Begin chatting below!\n");

  // initialize master and copy settings structs for select
  fd_set read_fds, read_fds_copy;
  FD_ZERO(&read_fds);
  FD_SET(STDIN, &read_fds);
  FD_SET(s, &read_fds);

  struct timeval timeout, timeout_copy;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  int sel;

  while (1) {
    read_fds_copy = read_fds; // since select edits the settings struct in place, need to make copy each time
    timeout_copy = timeout;
    sel = select(s + 1, &read_fds_copy, NULL, NULL, &timeout_copy);
    if (sel == -1) {
      perror("select:");
      exit(1);
    }
    if (sel == 0) { // timeout
      sendMessage(s, NULL); // send keepalive every 1 second
    }
    if (FD_ISSET(s, &read_fds_copy)) { // socket has input
      handleIncomingMessage(s);
    }
    if (FD_ISSET(STDIN, &read_fds_copy)) { // keyboard has input
      char input[1024];
      int len = read(STDIN, input, sizeof(input));
      input[len - 1] = '\0'; // minus 1 to get rid of newline
      if (strcmp(input, "#users") == 0) {
        printConnectedUsers();
      } else {
        sendMessage(s, input);
      }
    }
  }
  close(s);
}
