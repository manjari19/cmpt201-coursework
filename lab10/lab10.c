// server.c
/*
Understanding the Client:
1. How is the client sending data to the server? What protocol?
   - The client uses a TCP connection: it creates a socket with
     socket(AF_INET, SOCK_STREAM, 0), connects to the server with
     connect(), and then sends data using write() over that TCP stream.

2. What data is the client sending to the server?
   - It sends the 5 strings in the messages[] array:
       "Hello", "Apple", "Car", "Green", "Dog"
     in that order. Each message is copied into a buffer of size BUF_SIZE
     and written with write(sfd, buf, BUF_SIZE), so the server receives
     fixed-size 1024-byte messages padded with '\0'.

Understanding the Server:
1. Explain the argument that the `run_acceptor` thread is passed as an argument.
   - The argument is a pointer to a struct acceptor_args. That struct
     contains:
       - an atomic_bool run flag that tells the acceptor thread when to stop,
       - a pointer to the shared list_handle used to store received messages,
       - a pointer to the mutex (list_lock) that protects the list.
     main() initializes one acceptor_args instance and passes its address
     to pthread_create(), and run_acceptor() casts it back and uses it.

2. How are received messages stored?
   - Each client thread (run_client) reads messages into a local buffer,
     allocates a new struct list_node, allocates a data buffer for the
     message, copies the message into that buffer, and then calls
     add_to_list() to append the node to the end of a singly linked list.
     The list_handle keeps a pointer to the last node and a count of how
     many messages have been added.

3. What does `main()` do with the received messages?
   - main() waits until the list_handle.count indicates that enough
     messages have been received. Then it stops the acceptor thread,
     checks that the total number of messages equals
     MAX_CLIENTS * NUM_MSG_PER_CLIENT, and calls collect_all(head).
     collect_all() walks the list, prints each "Collected: <message>",
     frees the nodes and their data, and returns how many messages were
     collected. main() then reports whether all messages were collected.

4. How are threads used in this sample code?
   - There are multiple threads:
       - The main thread starts the acceptor thread and waits for enough
         messages to arrive.
       - The acceptor thread listens for incoming client connections on
         the server socket. For each client, it creates a client thread.
       - Each client thread handles reading from its client socket and
         appending received messages to the shared list in a thread-safe
         way. When shutting down, the acceptor thread signals client
         threads to stop and joins them.

Non-blocking sockets:
Explain the use of non-blocking sockets in this lab.
How are sockets made non-blocking?
What sockets are made non-blocking?
Why are these sockets made non-blocking? What purpose does it serve?

   - The sockets are made non-blocking so that the threads are not
     permanently stuck in accept() or read() calls. Instead of blocking
     forever, these calls return -1 with errno == EAGAIN or EWOULDBLOCK
     when there is no connection or data yet, and the thread can loop,
     check the run flag, and eventually shut down cleanly.

   - Sockets are made non-blocking using the set_non_blocking() helper:
       - It calls fcntl(fd, F_GETFL) to get the current flags and then
         fcntl(fd, F_SETFL, flags | O_NONBLOCK) to add the O_NONBLOCK flag.

   - The server socket (the listening socket) is made non-blocking in
     run_acceptor(), and each client connection socket (cfd) is made
     non-blocking in run_client().

   - This serves two main purposes:
       1) The acceptor thread can keep looping and check the run flag
          instead of being stuck forever in accept() when no clients
          are connecting.
       2) The client threads can keep looping and check the run flag
          instead of blocking forever in read() when the client stops
          sending data. This lets the program stop all threads gracefully.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define BUF_SIZE 1024
#define PORT 8001
#define LISTEN_BACKLOG 32
#define MAX_CLIENTS 4
#define NUM_MSG_PER_CLIENT 5

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

struct list_node {
  struct list_node *next;
  void *data;
};

struct list_handle {
  struct list_node *last;
  volatile uint32_t count;
};

struct client_args {
  atomic_bool run;

  int cfd;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

struct acceptor_args {
  atomic_bool run;

  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

int init_server_socket() {
  struct sockaddr_in addr;

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  return sfd;
}

// Set a file descriptor to non-blocking mode
void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL");
    exit(EXIT_FAILURE);
  }
}

void add_to_list(struct list_handle *list_handle, struct list_node *new_node) {
  struct list_node *last_node = list_handle->last;
  last_node->next = new_node;
  list_handle->last = last_node->next;
  list_handle->count++;
}

int collect_all(struct list_node head) {
  struct list_node *node = head.next; // get first node after head
  uint32_t total = 0;

  while (node != NULL) {
    printf("Collected: %s\n", (char *)node->data);
    total++;

    // Free node and advance to next item
    struct list_node *next = node->next;
    free(node->data);
    free(node);
    node = next;
  }

  return total;
}

static void *run_client(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  int cfd = cargs->cfd;
  set_non_blocking(cfd);

  char msg_buf[BUF_SIZE];

  while (cargs->run) {
    ssize_t bytes_read = read(cfd, &msg_buf, BUF_SIZE);
    if (bytes_read == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        perror("Problem reading from socket!\n");
        break;
      }
    } else if (bytes_read > 0) {
      // Create node with data
      struct list_node *new_node = malloc(sizeof(struct list_node));
      new_node->next = NULL;
      new_node->data = malloc(BUF_SIZE);
      memcpy(new_node->data, msg_buf, BUF_SIZE);

      struct list_handle *list_handle = cargs->list_handle;
      pthread_mutex_lock(cargs->list_lock);
      add_to_list(cargs->list_handle, new_node);
      pthread_mutex_unlock(cargs->list_lock);
    }
  }

  if (close(cfd) == -1) {
    perror("client thread close");
  }
  return NULL;
}

static void *run_acceptor(void *args) {
  int sfd = init_server_socket();
  set_non_blocking(sfd);

  struct acceptor_args *aargs = (struct acceptor_args *)args;
  pthread_t threads[MAX_CLIENTS];
  struct client_args client_args[MAX_CLIENTS];

  printf("Accepting clients...\n");

  uint16_t num_clients = 0;
  while (aargs->run) {
    if (num_clients < MAX_CLIENTS) {
      int cfd = accept(sfd, NULL, NULL);
      if (cfd == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          handle_error("accept");
        }
      } else {
        printf("Client connected!\n");

        client_args[num_clients].cfd = cfd;
        client_args[num_clients].run = true;
        client_args[num_clients].list_handle = aargs->list_handle;
        client_args[num_clients].list_lock = aargs->list_lock;
        num_clients++;
        pthread_create(&threads[num_clients - 1], NULL, run_client,
                       &client_args[num_clients - 1]);
      }
    }
  }

  printf("Not accepting any more clients!\n");

  // Shutdown and cleanup
  for (int i = 0; i < num_clients; i++) {
    client_args[i].run = false;
    pthread_join(threads[i], NULL);
    close(client_args[i].cfd);
  }

  if (close(sfd) == -1) {
    perror("closing server socket");
  }
  return NULL;
}

int main() {
  pthread_mutex_t list_mutex;
  pthread_mutex_init(&list_mutex, NULL);

  // List to store received messages
  // - Do not free list head (not dynamically allocated)
  struct list_node head = {NULL, NULL};
  struct list_node *last = &head;
  struct list_handle list_handle = {
      .last = &head,
      .count = 0,
  };

  pthread_t acceptor_thread;
  struct acceptor_args aargs = {
      .run = true,
      .list_handle = &list_handle,
      .list_lock = &list_mutex,
  };
  pthread_create(&acceptor_thread, NULL, run_acceptor, &aargs);

  while (1) {
    pthread_mutex_lock(&list_mutex);
    uint32_t count = list_handle.count;
    pthread_mutex_unlock(&list_mutex);

    if (count >= MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
      break;
    }
  }
  aargs.run = false;
  pthread_join(acceptor_thread, NULL);

  if (list_handle.count != MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
    printf("Not enough messages were received!\n");
    return 1;
  }

  int collected = collect_all(head);
  printf("Collected: %d\n", collected);
  if (collected != list_handle.count) {
    printf("Not all messages were collected!\n");
    return 1;
  } else {
    printf("All messages were collected!\n");
  }

  pthread_mutex_destroy(&list_mutex);

  return 0;
}

// client.c
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8001
#define BUF_SIZE 1024
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define NUM_MSG 5

static const char *messages[NUM_MSG] = {"Hello", "Apple", "Car", "Green",
                                        "Dog"};

int main() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("connect");
  }

  char buf[BUF_SIZE];
  for (int i = 0; i < NUM_MSG; i++) {
    sleep(1);
    // prepare message
    // this pads the desination with NULL
    strncpy(buf, messages[i], BUF_SIZE);

    if (write(sfd, buf, BUF_SIZE) == -1) {
      handle_error("write");
    } else {
      printf("Sent: %s\n", messages[i]);
    }
  }

  exit(EXIT_SUCCESS);
}
