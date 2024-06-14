#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
char *ParseBySpace(char *buffer, int buffer_size, int startAt);

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  int server_fd, client_addr_len;
  struct sockaddr_in client_addr;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(4221),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  int client =
      accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

  if (client == -1) {
    printf("Client accept failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Client connected\n");

  char buffer[1024];
  int bytes_read = read(client, buffer, 1024);
  if (bytes_read != -1) {
    char *op = ParseBySpace(buffer, bytes_read, 0);
    char *path = ParseBySpace(buffer, bytes_read, strlen(op) + 1);

    printf("Operation: %s\n", op);
    printf("Path: %s\n", path);

    if (strcmp(path, "/") == 0) {
      const char *response = "HTTP/1.1 200 OK\r\n\r\n";
      write(client, response, strlen(response));
    } else {
      const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
      write(client, response, strlen(response));
    }
  }

  close(server_fd);

  return 0;
}

char *ParseBySpace(char *buffer, int buffer_size, int startAt) {
  char *read_buffer = malloc(sizeof(char) * 5);

  for (int i = startAt; i < buffer_size && buffer[i] != ' '; i++) {
    read_buffer[i - startAt] = buffer[i];
  }

  return read_buffer;
}
