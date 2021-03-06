#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

typedef struct procstat
{
  int pid;
  char tty[32];
  char cmd[1024];
  char command[1024];
  char stat[8];
  unsigned long utime, stime;
  uid_t uid;
  char username[64];
  long nice;
  int session;
  long num_threads;
  int pgrp;
  int tpgid;
  long vsz;
  long rss;
  unsigned long starttime;
} procstat_t;

void print_ps(bool aflag, bool uflag, bool xflag);
bool isdigitstr(const char *str);
char *getttyfromproc(const char *pid);
void getstat(procstat_t *procstat, const char *pid);
void getstatus(char *result, const char *pid, const char *field);
void getmeminfo(char *result, const char *field);
void timeformat(char *result, unsigned long time, bool longFormat);
double cpupercent(time_t uptime, unsigned long utime, unsigned long stime, unsigned long starttime);
time_t uptime();