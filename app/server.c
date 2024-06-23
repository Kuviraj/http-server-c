#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct Request {
  char *method;
  char *path;
  char *version;
  char *user_agent;
  char *body;
};
char **path_input;
void make_empty_response(int *client_fd, int code, char *message) {
  char response_headers[] = "\r\n";
  char *response;
  asprintf(&response, "HTTP/1.1 %d %s\r\n\r\n", code, message);
  int sent = send(*client_fd, response, strlen(response), 0);
  send(*client_fd, response_headers, sizeof(response_headers), 0);
  free(response);
  return;
}

void make_text_response(int *client_fd, int code, char *message) {
  char *response;
  asprintf(&response,
           "HTTP/1.1 %d OK\r\nContent-Type: text/plain\r\nContent-Length: "
           "%lu\r\n\r\n%s",
           code, strlen(message), message);
  ssize_t sent = send(*client_fd, response, strlen(response), 0);
  free(response);
  return;
}

void make_file_response(int *client_id, char *file_path) {
  char *response;
  FILE *file_ptr = fopen(file_path, "r");
  /*int file_fd = open(file_path, O_RDONLY);*/
  if (file_ptr == NULL) {
    make_empty_response(client_id, 404, "Not Found");
    return;
  }
  char *file_contents = malloc(2048);
  int file_size = fread(file_contents, 1, 2048, file_ptr);

  printf("Using filecontents: %s\n", file_contents);
  printf("Size: %d\n", file_size);
  asprintf(&response,
           "HTTP/1.1 200 OK\r\nContent-Type: "
           "application/octet-stream\r\nContent-Length: %d\r\n\r\n%s",
           file_size, file_contents);
  ssize_t sent = send(*client_id, response, strlen(response), 0);
  free(response);
  free(file_contents);
  fclose(file_ptr);
  return;
}

int make_file(char *file_path, int *file_length, char *message) {
  printf("Message: %s\n", message);
  printf("File: %s\n", file_path);
  FILE *file_ptr = fopen(file_path, "w");
  fputs(message, file_ptr);
  fclose(file_ptr);
  
  return 0;
}

void *handle_request(void *arg) {
  int *client_fd = ((int *)arg);
  struct Request request;

  // create buffer to hold request
  char *buffer = (char *)malloc(1024 * sizeof(char));
  char *perm_buffer = (char*) malloc(1024 * sizeof(char));
  ssize_t bytes_received = recv(*client_fd, buffer, 1024, 0);
  if (bytes_received < 0) {
    printf("Recieve failed: %zd", bytes_received);
    return NULL;
  }
  strcpy(perm_buffer, buffer);
  // parse request
  request.method = strtok(buffer, " ");
  printf("Method: %s\n", request.method);
  request.path = strtok(NULL, " ");
  printf("Path: %s\n", request.path);
  request.version = strtok(NULL, "\r\n");
  printf("Host: %s\n", strtok(NULL, "\r\n") + 6);

  // routing
  if (strcmp(request.path + 1, "") == 0) {
    make_empty_response(client_fd, 200, "OK");
  } else if (strncmp(request.path + 1, "echo", 4) == 0) {
    printf("Running echo\n");
    char *text = request.path + 6;
    make_text_response(client_fd, 200, text);
  } else if (strcmp(request.path, "/user-agent") == 0) {
    request.user_agent = strtok(NULL, "\r\n") + 12;
    printf("User agent: %s\n", request.user_agent);
    make_text_response(client_fd, 200, request.user_agent);
  } else if (strncmp(request.path, "/files", 6) == 0) {
    if (strcmp(request.method, "GET") == 0) {
      if (path_input == NULL) {
        make_empty_response(client_fd, 404, "Not Found");
        return NULL;
      }
      char *path = malloc(50);

      strcpy(path, *path_input);
      strcat(path, (request.path + 7));
      printf("File: %s\n", path);

      make_file_response(client_fd, path);
      free(path);
    } else if (strcmp(request.method, "POST") == 0) {
      printf("Messae: %s", perm_buffer);
      char *path = malloc(50);

      strcpy(path, *path_input);
      strcat(path, (request.path + 7));
      printf("File1: %s\n", path);

      
      strtok(NULL, "\r\n");
      /*strtok(NULL, "\r\n");*/
      /*strtok(NULL, "\r\n");*/
      char *buffer_length = strtok(NULL, "\r\n") + 16;
      int length = atoi(buffer_length);
      printf("Length: %d\n", length);
      request.body = strtok(NULL, "\r\n");
      printf("Data: %s\n", request.body);
      if (make_file(path, &length, request.body) == 0) {

        make_empty_response(client_fd, 201, "Created");
      }
    }
  } else {
    make_empty_response(client_fd, 404, "Not Found");
  }
  free(buffer);
  close(*client_fd);
  return NULL;
}

int main(int argc, char **argv) {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  printf("Logs from your program will appear here!\n");

  // Uncomment this block to pass the first stage
  //
  int server_fd;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }
  //
  // // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // // ensures that we don't run into 'Address already in use' errors
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

  if (argc > 1) {
    path_input = (argv + 2);
    printf("Using filepath: %s\n", *path_input);
  }
  while (1) {
    struct sockaddr_in client_addr;
    printf("Waiting for a client to connect...\n");
    socklen_t client_addr_len = sizeof(client_addr);
    int *client_fd = malloc(sizeof(int));
    if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                             &client_addr_len)) < 0) {
      printf("Accept failed\n");
      continue;
    }

    printf("Client connected\n");
    /*handle_request((void*)client_fd);*/
    pthread_t thread_id;
    int err = pthread_create(&thread_id, 0, handle_request, (void *)client_fd);
    if (err) {
      printf("Thread failed\n");
    }
    pthread_detach(thread_id);
    // Send response
    printf("Sent response\n");
  }

  close(server_fd);
  return 0;
}
