#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct Response {
  char *method;
  char *path;
};

void make_response(int *client_fd, int code, char *message) {
  char response_headers[] = "\r\n";
  char *response;
  asprintf(&response, "HTTP/1.1 %d %s\r\n\r\n", code, message);
  int sent = send(*client_fd, response, strlen(response), 0);
  send(*client_fd, response_headers, sizeof(response_headers), 0);
  free(response);
}

void handle_request(int *client_fd) {

  struct Response response;

  char *buffer = (char *)malloc(1024 * sizeof(char));
  ssize_t bytes_received = recv(*client_fd, buffer, 1024, 0);

  response.method = strtok(buffer, " ");
  printf("Method: %s\n", response.method);
  response.path = strtok(NULL, " ");
  printf("Path: %s\n", response.path);
  if (strcmp(response.path + 1, "") == 0) {
    make_response(client_fd, 200, "OK");
  } else {
        make_response(client_fd, 404, "Not Found");
    }

}

int main() {
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

  while (1) {
    int client_addr_len;
    struct sockaddr_in client_addr;
    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    printf("Client connected\n");
    handle_request(&client_fd);

    // Send response

    close(client_fd);
  }

  close(server_fd);
  return 0;
}
