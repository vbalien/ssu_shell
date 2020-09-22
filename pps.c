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
    if (getstat(stat[count], pid) == false)
      continue;

    if (!aflag && stat[count]->uid != getuid())
      continue;

    if (strchr(stat[count]->stat, 'I') != NULL)
    {
      free(stat[count]);
      continue;
    }

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
             "0.0", // TODO
             "0.0", // TODO
             0,     // TODO
             0,     // TODO
             stat[i]->tty,
             stat[i]->stat,
             "00:00", // TODO
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

bool getstat(procstat_t *result, const char *pid)
{
  FILE *fp;
  char tmp[1024];
  int i;
  char *tty;
  struct passwd *pws;
  struct stat st;

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

  // 14번 필드인 stime을 얻기 위해 10번 필드를 건너뜀
  for (i = 0; i < 11; i++)
    fscanf(fp, "%lu", &result->stime);
  // cutime 읽음
  fscanf(fp, "%lu", &result->time);
  fclose(fp);

  // cmdline을 읽음
  sprintf(tmp, "/proc/%s/cmdline", pid);
  fp = fopen(tmp, "r");
  fread(result->command, 1, 1024, fp);
  fclose(fp);

  if ((pws = getpwuid(st.st_uid)) != NULL)
    strcpy(result->username, pws->pw_name);
  else
    return false;
  return true;
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
