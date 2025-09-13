#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char *line = NULL; // getline would allocate this buffer
  size_t cap = 0;    // this determines the capacity of the buffer
  ssize_t nread;     // this shows the number of characters read

  printf("Please enter some text: ");
  nread = getline(&line, &cap, stdin);

  if (nread == -1) {
    perror("getline");
    free(line);
    return 1;
  }

  puts("Tokens:");

  // split on spaces tabs and newlines

  char *save = NULL;
  char *tok = strtok_r(line, " \t\n", &save);
  while (tok) {
    printf(" %s\n", tok);
    tok = strtok_r(NULL, " \t\n", &save);
  }

  free(line); // to cleanupp
  return 0;
}
