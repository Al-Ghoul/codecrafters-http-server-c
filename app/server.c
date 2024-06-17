#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void *handle_connection(void *client_);
char *ParseByCharacter(char *buffer, int buffer_size, int startAt, char c);

char *directory = NULL;
int main(int argc, char *argv[]) {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  if (argc > 1) {
    const char *arg = "--directory";
    if (strcmp(argv[1], arg) == 0) {
      directory = argv[2];
      printf("Directory: '%s'\n", directory);
    }
  }

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

  while (1) {
    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_fd == -1) {
      printf("Client accept failed: %s \n", strerror(errno));
      return 1;
    }

    pthread_t thread_id;
    int rc =
        pthread_create(&thread_id, NULL, handle_connection, (void *)&client_fd);
    if (rc) {
      printf("Thread creation failed: %s \n", strerror(errno));
      return 1;
    }
  }
  printf("Client connected\n");
  close(server_fd);

  return 0;
}

void *handle_connection(void *client_) {
  int client_fd = *(int *)(client_);
  char buffer[1024];
  int bytes_read = read(client_fd, buffer, 1024);
  if (bytes_read != -1) {
    printf("Received: %s\n", buffer);
    char *method = ParseByCharacter(buffer, bytes_read, 0, ' ');
    char *wholePath =
        ParseByCharacter(buffer, bytes_read, strlen(method) + 2, ' ');
    char *firstPath = ParseByCharacter(wholePath, strlen(wholePath), 0, '/');

    printf("Method: '%s'\n", method);
    printf("Whole path: '%s'\n", wholePath);
    printf("Path: '%s'\n", firstPath);

    char *pch = strchr(wholePath, '/');
    char *subPath = NULL;
    if (pch != NULL) {
      subPath = ParseByCharacter(wholePath, strlen(wholePath),
                                 pch - wholePath + 1, '/');
      printf("Subpath: '%s'\n", subPath);
    }

    if (strcmp(firstPath, "") == 0) {
      const char *response = "HTTP/1.1 200 OK\r\n\r\n";
      write(client_fd, response, strlen(response));
    } else if (strcmp(firstPath, "user-agent") == 0) {
      const char *header = "User-Agent:";
      char *headerkey = strstr(buffer, header);
      if (headerkey != NULL) {
        const char *headerValue = ParseByCharacter(
            buffer, bytes_read, headerkey - buffer + strlen(header) + 1, '\r');
        printf("'%s'", headerValue);

        char response[256];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Type: "
                 "text/plain\r\nContent-Length: %lu\r\n\r\n%s",
                 strlen(headerValue), headerValue);
        free((char *)headerValue);

        write(client_fd, response, strlen(response));
      }
    } else if (strcmp(firstPath, "echo") == 0) {

      char response[256];
      snprintf(response, sizeof(response),
               "HTTP/1.1 200 OK\r\nContent-Type: "
               "text/plain\r\nContent-Length: %lu\r\n\r\n%s",
               strlen(subPath), subPath);

      write(client_fd, response, strlen(response));
    } else if (strcmp(firstPath, "files") == 0) {
      if (strcmp(method, "GET") == 0) {
        FILE *file = NULL;
        char *temp =
            malloc(sizeof(char) * strlen(directory) + strlen(subPath) + 1);
        char response[256];
        strcpy(temp, directory);
        file = fopen(strcat(temp, subPath), "rb");

        if (file != NULL) {
          char *tempBuffer = NULL;
          fseek(file, 0, SEEK_END);
          long fileSize = ftell(file);
          fseek(file, 0, SEEK_SET);

          tempBuffer = malloc(fileSize);
          if (tempBuffer) {
            fread(tempBuffer, 1, fileSize, file);
          }
          snprintf(response, sizeof(response),
                   "HTTP/1.1 200 OK\r\nContent-Type: "
                   "application/octet-stream\r\nContent-Length: %lu\r\n\r\n%s",
                   fileSize, tempBuffer);
          // Close the file
          fclose(file);
          file = NULL;
          printf("File %s closed\n", subPath);
        } else {
          snprintf(response, sizeof(response),
                   "HTTP/1.1 404 Not Found\r\n\r\n");
        }

        free(temp);

        write(client_fd, response, strlen(response));
      } else if (strcmp(method, "POST") == 0) {
        const char *header = "Content-Length:";
        char *headerkey = strstr(buffer, header);
        if (headerkey != NULL) {
          const char *headerValue =
              ParseByCharacter(buffer, bytes_read,
                               headerkey - buffer + strlen(header) + 1, '\r');
          const char *content = ParseByCharacter(
              buffer, bytes_read,
              headerkey - buffer + strlen(headerValue) + strlen(header) + 5,
              '\n');
          char *tempBuffer = malloc(sizeof(char) * (int)(*headerValue - '0'));
          printf("'%s'\n", content);

          FILE *file = NULL;

          char *temp =
              malloc(sizeof(char) * strlen(directory) + strlen(subPath) + 1);
          strcpy(temp, directory);
          file = fopen(strcat(temp, subPath), "w");

          fprintf(file, "%s", content);

          fclose(file);
          free(temp);
          free(tempBuffer);

          const char *response = "HTTP/1.1 201 Created\r\n\r\n";
          write(client_fd, response, strlen(response));
        }
      }

    } else {
      const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
      write(client_fd, response, strlen(response));
    }

    free(method);
    free(wholePath);
    free(firstPath);
    free(subPath);
  }

  printf("Closing connection\n");
  close(client_fd);

  return NULL;
}

char *ParseByCharacter(char *buffer, int buffer_size, int startAt, char c) {
  char *read_buffer = malloc(sizeof(char) * 255);

  int i;
  for (i = startAt; i < buffer_size && buffer[i] != c; i++) {
    read_buffer[i - startAt] = buffer[i];
  }

  read_buffer[i - startAt] = '\0';

  return read_buffer;
}
