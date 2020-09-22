#include "pps.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
  int i;
  bool aflag = false, uflag = false, xflag = false;
  int x, y;

  // 스크린 사이즈를 구함
  initscr();
  getmaxyx(stdscr, y, x);
  endwin();

  // option 파싱
  if (argc > 1)
    for (i = 0; argv[1][i]; ++i)
    {
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

void print_ps(bool aflag, bool uflag, bool xflag)
{
  DIR *dir;
  int i;
  struct dirent *entry;
  char tty_self[1024];
  char *tty;
  char *pid;
  procstat_t *stat[1024];
  int count = 0;
  char tmp[1024];
  int pidwidth = 0;

  // 현재 tty를 가져옴
  strcpy(tty_self, getttyfromproc("self"));

  // 프로세스 목록을 가져옴
  dir = opendir("/proc");
  while ((entry = readdir(dir)) != NULL)
  {
    // 숫자가 아닐 경우는 무시함
    if (!isdigitstr(entry->d_name))
      continue;
    pid = entry->d_name;

    // 프로세스의 tty를 가져 옴
    tty = getttyfromproc(pid);

    // x플래그가 없으면 tty가 없는건 무시함
    if (!xflag && !tty)
      continue;

    if (!aflag && !uflag && !xflag)
      if (tty && strcmp(tty_self, tty) != 0)
        continue;

    stat[count] = (procstat_t *)malloc(sizeof(procstat_t));
    getstat(stat[count], pid);

    if (!aflag && stat[count]->uid != getuid())
      continue;

    if (strlen(pid) > pidwidth)
      pidwidth = strlen(pid);

    ++count;
  }

  if (pidwidth < 5)
    pidwidth = 5;

  if (uflag)
  {
    printf("%-8s %*s %4s %4s %6s %5s %-8s %-4s %-5s %6s %s\n", "USER", pidwidth, "PID", "%CPU", "%MEM", "VSZ", "RSS", "TTY", "STAT", "START", "TIME", "COMMAND");
    for (i = 0; i < count; ++i)
    {
      timeformat(tmp, stat[i]->time + stat[i]->stime, false);
      printf("%-8s %*d %4s %4s %6d %5d %-8s %-4s %-5s %6s %s\n",
             stat[i]->username,
             pidwidth, stat[i]->pid,
             "0.0", // TODO %CPU
             "0.0", // TODO %MEM
             0,     // TODO VSZ
             0,     // TODO RSS
             stat[i]->tty,
             stat[i]->stat,
             "00:00", // TODO START
             tmp,
             stat[i]->command);
    }
  }
  else if (aflag || xflag)
  {
    printf("%*s %-8s %-6s %4s %s\n", pidwidth, "PID", "TTY", "STAT", "TIME", "COMMAND");
    for (i = 0; i < count; ++i)
    {
      timeformat(tmp, stat[i]->time + stat[i]->stime, false);
      printf("%*d %-8s %-6s %4s %s\n", pidwidth, stat[i]->pid, stat[i]->tty, stat[i]->stat, tmp, stat[i]->command);
    }
  }
  else
  {
    printf("%*s %-8s %8s %s\n", pidwidth, "PID", "TTY", "TIME", "CMD");
    for (i = 0; i < count; ++i)
    {
      timeformat(tmp, stat[i]->time + stat[i]->stime, true);
      printf("%*d %-8s %8s %s\n", pidwidth, stat[i]->pid, stat[i]->tty, tmp, stat[i]->cmd);
    }
  }
}

bool isdigitstr(const char *str)
{
  int i;
  for (i = 0; str[i]; i++)
    if (!isdigit(str[i]))
      return false;
  return true;
}

char *getttyfromproc(const char *pid)
{
  static char tty[64];
  char path[1024];
  int fd;
  // proc의 fd경로를 가져옴
  sprintf(path, "/proc/%s/fd/0", pid);
  fd = open(path, O_RDONLY);
  if (!isatty(fd))
  {
    close(fd);
    return NULL;
  }
  close(fd);
  if (readlink(path, tty, 64) == -1)
    return NULL;
  return tty;
}

void getstat(procstat_t *result, const char *pid)
{
  FILE *fp;
  char tmp[1024];
  int i;
  char *tty;
  int ttyfd;
  struct passwd *pws;
  struct stat st;
  size_t readsize;

  bzero(result, sizeof(procstat_t));

  tty = getttyfromproc(pid);
  if (tty != NULL)
    strcpy(result->tty, tty + 5);
  else
    strcpy(result->tty, "?");

  sprintf(tmp, "/proc/%s/stat", pid);

  stat(tmp, &st);
  result->uid = st.st_uid;

  fp = fopen(tmp, "r");
  // 3번째 필드까지 읽음
  fscanf(fp, "%d%s%s", &result->pid, tmp, &result->stat);
  // cmd의 양쪽 괄호를 제거
  strcpy(result->cmd, tmp + 1);
  result->cmd[strlen(result->cmd) - 1] = 0;

  // Lock State 구함
  getstatus(tmp, pid, "VmLck");
  if (strlen(tmp) != 0)
  {
    sscanf(tmp, "%d", &i);
    if (i != 0)
      strcat(result->stat, "L");
  }

  // (5) pgrp를 얻기 위해 1번 필드를 건너뜀
  for (i = 0; i < 2; i++)
    fscanf(fp, "%d", &result->pgrp);

  // (6) session
  fscanf(fp, "%d", &result->session);
  if (result->session == result->pid)
    strcat(result->stat, "s");

  // (8) tpgrp
  for (i = 0; i < 2; i++)
    fscanf(fp, "%d", &result->tpgid);

  // (14) stime을 얻기 위해 5번 필드를 건너뜀
  for (i = 0; i < 6; i++)
    fscanf(fp, "%lu", &result->stime);
  // (15) cutime 읽음
  fscanf(fp, "%lu", &result->time);

  // (19) nice를 얻기 위해 3번 필드를 건너뜀
  for (i = 0; i < 4; i++)
    fscanf(fp, "%ld", &result->nice);

  // (20) num_threads
  fscanf(fp, "%ld", &result->num_threads);
  if (result->num_threads > 1)
    strcat(result->stat, "l");

  if (result->nice < 0)
    strcat(result->stat, "<");
  else if (result->nice > 0)
    strcat(result->stat, "N");

  fclose(fp);

  // cmdline을 읽음
  sprintf(tmp, "/proc/%s/cmdline", pid);
  fp = fopen(tmp, "r");
  readsize = fread(result->command, 1, 1024, fp);
  // NULL을 공백으로 치환
  for (i = 0; i < readsize; ++i)
    if (result->command[i] == 0)
      result->command[i] = ' ';

  fclose(fp);

  if (strlen(result->command) == 0)
    sprintf(result->command, "[%s]", result->cmd);

  if (result->tpgid != -1)
    strcat(result->stat, "+");

  // username얻기
  if ((pws = getpwuid(st.st_uid)) != NULL)
    strcpy(result->username, pws->pw_name);
  else
    sprintf(result->username, "%d", st.st_uid);
}

void timeformat(char *result, unsigned long time, bool longFormat)
{
  bzero(result, strlen(result));
  time = (int)(time / sysconf(_SC_CLK_TCK));

  if (longFormat)
    sprintf(result, "%02lu:%02lu:%02lu", (time / 3600) % 3600, (time / 60) % 60,
            time % 60);
  else
    sprintf(result, "%01lu:%02lu", (time / 60) % 60, time % 60);
}

void getstatus(char *result, const char *pid, const char *field)
{
  char path[1024];
  char name[1024];
  char value[1024];
  FILE *fp;
  size_t read;
  char *line = NULL;
  size_t len = 0;
  int i;

  bzero(result, strlen(result));
  sprintf(path, "/proc/%s/status", pid);
  fp = fopen(path, "r");
  while ((read = getline(&line, &len, fp)) != -1)
  {
    sscanf(line, "%[^:]:%[^\n]", name, value);
    if (strcmp(field, name) == 0)
    {
      for (i = 0; value[i] == '\t' || value[i] == ' '; ++i)
        ;
      strcpy(result, value + i);
      free(line);
      return;
    }
  }
  free(line);
}