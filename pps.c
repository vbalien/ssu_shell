#include "pps.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int i;
  bool aflag = false, uflag = false, xflag = false;
  int x, y;

  // 스크린 사이즈를 구함
  initscr();
  getmaxyx(stdscr, y, x);
  endwin();

  // option 파싱
  if (argc > 1)
    for (i = 0; argv[1][i]; ++i) {
      if (argv[1][i] == 'a')
        aflag = true;
      if (argv[1][i] == 'u')
        uflag = true;
      if (argv[1][i] == 'x')
        xflag = true;
    }

  // ps 출력
  print_ps(aflag, uflag, xflag);
  return 0;
}

void print_ps(bool aflag, bool uflag, bool xflag) {
  DIR *dir;
  int i;
  struct dirent *entry;

  // 프로세스 목록을 가져옴
  dir = opendir("/proc");
  while ((entry = readdir(dir)) != NULL) {
    // 숫자가 아닐 경우는 무시함
    if (!isdigitstr(entry->d_name))
      continue;
    // tty를 가져 옴
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
  // proc의 fd경로를 가져옴
  sprintf(path, "/proc/%s/fd/0", proc);
  // fd를 열어서 tty를 가져와 리턴
  fd = open(path, O_RDONLY);
  return ttyname(fd);
}
