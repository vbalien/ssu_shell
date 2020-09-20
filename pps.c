#include "pps.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>

int main() {
  int x, y;
  initscr();
  getmaxyx(stdscr, y, x);
  endwin();
  print_ps();
  return 0;
}

void print_ps() {
  DIR *dir;
  int i;
  struct dirent *entry;

  // get directory list
  dir = opendir("/proc");

  while ((entry = readdir(dir)) != NULL) {
    if (!isdigitstr(entry->d_name))
      continue;
    printf("TTY: %s\n", getttyfromproc(entry->d_name));
    // printf("%s\n", entry->d_name);
  }
}

bool isdigitstr(char *str) {
  int i;
  for (i = 0; str[i]; i++)
    if (!isdigit(str[i]))
      return false;
  return true;
}

char *getttyfromproc(char *proc) {
  char path[1024];
  int fd;
  sprintf(path, "/proc/%s/fd/0", proc);
  fd = open(path, O_RDONLY);
  return ttyname(fd);
}
