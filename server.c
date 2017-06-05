#include <fcntl.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <unistd.h>

#define MY_PORT 6968
#define STDIN 0
#define MAX_CLIENT_COUNT 500
#define MAX_USERNAME_LENGTH 256
FILE *f;
char filename[32];

struct clientinfo {
  int sockdesc;                   // socket descriptor for this client
  char name[MAX_USERNAME_LENGTH]; // username of this client
  int connected;                  // 1: is connected, 0: is not connected
  long long time_left;
};

struct clientinfo clientlist[MAX_CLIENT_COUNT]; // array of clientinfo structs

void sigint_handler(int signo){
  fprintf(f,"Terminating...\n");
  fclose(f);
  exit(0);
}

// Disconnects the client at index i in clientlist
void disconnectClient(int i) {
  close(clientlist[i].sockdesc);
  int j;
  for (j = 0; j < MAX_CLIENT_COUNT; j++) {
    if (clientlist[j].connected == 1) {
      uint8_t msg_type = 0x02;
      uint8_t username_len = strlen(clientlist[i].name);
      write(clientlist[j].sockdesc, &msg_type, sizeof(msg_type));
      write(clientlist[j].sockdesc, &username_len, sizeof(username_len));
      write(clientlist[j].sockdesc, clientlist[i].name, username_len);
    }
  }
  fprintf(f,"%s has disconnected!\n",clientlist[i].name);
  clientlist[i].connected = 0;
  clientlist[i].sockdesc = 0;
  strcpy(clientlist[i].name, "\0");
  clientlist[i].time_left = 0.0;
}

void handleClientTimeouts(long long elapsed) {
  int i;
  for (i = 0; i < MAX_CLIENT_COUNT; i++) {
    if (clientlist[i].connected == 1) {
      // decrease the time for the particular struct by 1 s.
      long long time = clientlist[i].time_left;
      // if the time is currently 0, close the socket
      if (time <= 0) {
        disconnectClient(i);
      } else {
        time = time - elapsed;
        clientlist[i].time_left = time;
      }
    }
  }
}


int main(int argc, char *argv[]) {

  //setting up handler
  struct sigaction sigint_act;
  sigint_act.sa_handler = sigint_handler;
  sigemptyset(&sigint_act.sa_mask);
  sigaction(SIGINT, &sigint_act, NULL);

  //getting the process id
  //server379procid.log
  int pid = getpid();
  snprintf(filename, sizeof(char) * 32, "server379%i.log", pid);
  //printf("log file name: %s\n",filename);
  f = fopen(filename, "w");

  printf("Initializing chat server...\n");

  // get port number
  int port = 6969;
  if (argc != 2) {
    printf("Using default port: %d\n", port);
    printf("To specify a custom port, use \"server <port>\"\n");
  } else {
    port = atoi(argv[1]);
    printf("Using port: %d\n", port);
  }

  // initializing all of the values to 0
  int i;
  for (i = 0; i < MAX_CLIENT_COUNT; i++) {
    clientlist[i].connected = 0;
    clientlist[i].sockdesc = 0;
    strcpy(clientlist[i].name, "\0");
    clientlist[i].time_left = 0;
  }

  // set up master socket
  int sock, snew, fromlength;
  struct sockaddr_in master, from;
  fromlength = sizeof(from);
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Server: cannot open master socket");
    exit(1);
  }
  master.sin_family = AF_INET;
  master.sin_addr.s_addr = INADDR_ANY;
  master.sin_port = htons(port);
  if (bind(sock, (struct sockaddr *)&master, sizeof(master))) {
    perror("Server: cannot bind master socket");
    exit(1);
  }
  listen(sock, 5);
  // end set up master socket

  // set up select on master socket
  fd_set sock_fds, sock_fds_copy;
  FD_ZERO(&sock_fds);
  FD_SET(sock, &sock_fds);
  struct timeval sock_timeout, sock_timeout_copy;
  sock_timeout.tv_sec = 0;
  sock_timeout.tv_usec = 1000;
  // end set up select on master socket

  struct timeval t0, t1;
  gettimeofday(&t0, NULL);

  printf("Chat server successfully initialized!\n");

  while (1) {

    gettimeofday(&t1, NULL);
    // find time elapsed and decrement time left on clients
    long long elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
    t0 = t1;
    handleClientTimeouts(elapsed);

    sock_fds_copy = sock_fds;
    sock_timeout_copy = sock_timeout;

    int sock_sel = select(sock + 1, &sock_fds_copy, NULL, NULL, &sock_timeout_copy);
    if (sock_sel == -1) {
      perror("sock_sel select:");
      exit(1);
    }
    if (sock_sel == 0) { // timeout occured - no new clients

      // check input from any of the clients
      fd_set read_fds;
      FD_ZERO(&read_fds);
      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      // getting the highest file descriptor value
      int max_fd = 0;
      for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clientlist[i].connected == 1) {
          FD_SET(clientlist[i].sockdesc, &read_fds);
          if (clientlist[i].sockdesc > max_fd) {
            max_fd = clientlist[i].sockdesc;
          }
        }
      }

      int sel = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

      if (sel == -1) {
        perror("sel select:");
      }

      if (sel == 0) {
        continue;
      }

      for (i = 0; i < MAX_CLIENT_COUNT; i++) {

        if (clientlist[i].connected == 1 &&
            FD_ISSET(clientlist[i].sockdesc,
                     &read_fds)) // client socket has input
        {

          uint16_t msg_len;
          int nbytes = read(clientlist[i].sockdesc, &msg_len, sizeof(msg_len));
	  if (nbytes == 0) {
	    disconnectClient(i);
	    continue;
  	  }
          uint16_t msg_lenh = ntohs(msg_len);

          clientlist[i].time_left = 30 * 1000 * 1000;

          // client has sent a keepalive message
          if (msg_lenh == 0) {
            continue;
          }

          char msg[msg_lenh + 1];
          read(clientlist[i].sockdesc, msg, msg_lenh);
          msg[msg_lenh] = '\0';

          // relay the message to all connected clients
          uint8_t msg_type = 0x00;
          uint8_t name_len = strlen(clientlist[i].name);
          int j;
          int counter = 0;
          for (j = 0; j < MAX_CLIENT_COUNT; j++) {
            if (clientlist[j].connected == 1) {
              write(clientlist[j].sockdesc, &msg_type, sizeof(msg_type));
              write(clientlist[j].sockdesc, &name_len, sizeof(name_len));
              write(clientlist[j].sockdesc, clientlist[i].name, name_len);
              write(clientlist[j].sockdesc, &msg_len,sizeof(msg_len)); // sending size
              write(clientlist[j].sockdesc, msg, msg_lenh); // sending messaqge
	      if(counter == 0){
	        fprintf(f,"%s sent a message: %s\n",clientlist[i].name, msg);
              }
	      counter++;
            }
          }
        }
      }
    }
    if (FD_ISSET(sock, &sock_fds_copy)) { // new client wants to join

      // get file descriptor of socket for new client
      snew = accept(sock, (struct sockaddr *)&from, (socklen_t *)&fromlength);
      fprintf(f,"Client accepted on socket descriptor: %d\n", snew);

      // send handshake bytes
      uint8_t byte1 = 0xCF;
      uint8_t byte2 = 0xA7;
      write(snew, &byte1, sizeof(byte1));
      write(snew, &byte2, sizeof(byte2));

      // count number of users (other than current user) currently connected
      uint16_t clientcount = 0;
      for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clientlist[i].connected == 1) {
          clientcount++;
        }
      }
      clientcount = htons(clientcount);
      write(snew, &clientcount, sizeof(clientcount));

      // send connected users information to new client
      for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clientlist[i].connected == 1) {
          uint8_t num_chars = strlen(clientlist[i].name);
          write(snew, &num_chars, sizeof(num_chars));
          write(snew, clientlist[i].name, num_chars);
        }
      }

      // get username from newly connected client
      uint8_t len;
      read(snew, &len, sizeof(len));
      char name[len + 1];
      read(snew, name, len);
      name[len] = '\0';
      fprintf(f,"Newly accepted client's username: %s\n",name);

      // check if username is already being used
      int invalid = 0;
      for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (strcmp(name, clientlist[i].name) == 0) {
          fprintf(f,"Name (%s) is already in the chat.\n", name);
          close(snew); // close connection if usename being used
          invalid = 1;
        }
      }
      if (invalid == 1)
        continue;

      // tell other users that a new user is joining
      for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clientlist[i].connected == 1) {
          uint8_t msg_type = 0x01;
          write(clientlist[i].sockdesc, &msg_type, sizeof(msg_type));
          write(clientlist[i].sockdesc, &len, sizeof(len));
          write(clientlist[i].sockdesc, name, len);
        }
      }

      // if everything is okay, set up a client info struct for the new client
      for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clientlist[i].connected == 0) {
          clientlist[i].connected = 1;
          clientlist[i].time_left = 60 * 10 * 1000 * 1000; // 10 mins in microseconds
          clientlist[i].sockdesc = snew;
          strcpy(clientlist[i].name, name);
          break;
        }
        if (i == MAX_CLIENT_COUNT - 1 ) {
          fprintf(f,"Client list is full\n");
        }
      }
    }
  } // while
} // main
