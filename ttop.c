#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

// 프로세스 정보를 담는 구조체
typedef struct proc
{
  int pid;
  char username[32];
  long priority;
  long nice;
  unsigned long vsize;
  unsigned long rss;
  unsigned long shr;
  char stat[2];
  double cpu;
  double mem;
  char time[1024];
  char command[1024];
} proc_t;

// SIGINT 핸들러
void handler(int sig);

// 헤더영역 그리는 함수
void draw_header();
// 프로세스 테이블 그리는 함수
void draw_table();

// 커서를 다음줄로 옮기는 함수
void nextline();

// proc 목록을 초기화하는 함수
void readproc();

// proc목록을 해제하는 함수
void freeproc();

// %CPU 구하는 함수
double cpupercent(time_t uptime, unsigned long utime, unsigned long stime, unsigned long starttime);

// uptime 구하는 함수
time_t uptime();

// 문자열 숫자 체크 함수
bool isdigitstr(const char *str);

// meminfo 가져오는 함수
void getmeminfo(char *result, const char *field);

int ch;
bool redraw = true;
int offset = 0;
int w, h;
proc_t *procs[1024];
int proccount = 0;

long memtotal = 0;
long memfree = 0;
long memavail = 0;
long membuf = 0;
long memcache = 0;

long swaptotal = 0;
long swapfree = 0;

time_t old = 0;
time_t now = 0;

int main()
{
  // 정상적인 종료를 위해 핸들링함
  signal(SIGINT, handler);

  initscr();             // ncurses 초기화
  refresh();             // 화면을 비움
  noecho();              // 화면에 입력을 출력하지 않음
  cbreak();              // 버퍼링 사용하지 않음
  nodelay(stdscr, true); // 입력 딜레이 없이 함
  curs_set(0);           // 커서를 없앰
  keypad(stdscr, TRUE);  // 특수키를 입력받도록 함
  start_color();         // 색상 사용

  init_pair(1, COLOR_BLUE, COLOR_RED);

  while (true)
  {
    if ((ch = getch()) != ERR)
    {
      if (ch == 'q') // q입력시 종료
        break;
      else if (ch == KEY_UP) // 오프셋 위로 이동
      {
        --offset;
        redraw = true; // 새로 그림
      }
      else if (ch == KEY_DOWN) // 오프셋 아래로 이동
      {
        ++offset;
        redraw = true; // 새로 그림
      }
    }

    // 3초 지나면 갱신시켜줌
    time(&now);
    if (now - old == 3)
      redraw = true;

    // 다시 그릴 필요가 있을 때만 그린다.
    if (redraw)
    {
      // 시간 업데이트
      old = now;

      redraw = false;
      attron(A_STANDOUT);
      getmaxyx(stdscr, h, w);
      readproc();
      draw_header();
      nextline();
      draw_table();
      freeproc();
    }
    usleep(10000L); // 10ms만큼 쉼
  }

  // endwin 안하면 터미널 이상해짐
  endwin();
  exit(0);
}

void draw_header()
{
  FILE *fp;
  int i;
  int usercount = 0;
  float loadaverages[3] = {
      0.f,
  };
  int rcount = 0, slcount = 0, stcount = 0, zcount = 0;
  char nowstr[100];
  char upstr[100];
  time_t rawtime;
  struct tm *timeinfo;
  int updays;
  long upsecs, upmins, uphours;
  int pos = 0;
  long cputime[8];
  double cputotal = .0;

  // uptime 구함
  bzero(upstr, 100);
  upsecs = uptime();
  updays = upsecs / (60 * 60 * 24);

  if (updays)
    pos += sprintf(upstr, "%d days, ", updays);

  upmins = (int)upsecs / 60;
  uphours = (int)upmins / 60;
  uphours = (int)uphours % 24;
  upmins = upmins % 60;

  if (uphours)
    pos += sprintf(upstr + pos, "%2ld:%2ld", uphours, upmins);
  else
    pos += sprintf(upstr + pos, "%ld min", upmins);

  // loadavg 구함
  fp = fopen("/proc/loadavg", "r");
  fscanf(fp, "%f%f%f", &loadaverages[0], &loadaverages[1], &loadaverages[2]);
  fclose(fp);

  // usercount구함
  fp = fopen("/proc/key-users", "r");
  while (!feof(fp))
    if (fgetc(fp) == '\n')
      usercount++;
  fclose(fp);

  // 현재시각 구함
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(nowstr, 100, "%T", timeinfo);

  // CPU사용량 구함
  fp = fopen("/proc/stat", "r");
  fscanf(fp, "%*s%ld%ld%ld%ld%ld%ld%ld%ld",
         &cputime[0],  // user
         &cputime[1],  // nice
         &cputime[2],  // sys
         &cputime[3],  // idle
         &cputime[4],  // wait
         &cputime[5],  // hi
         &cputime[6],  // si
         &cputime[7]); // st
  fclose(fp);

  for (i = 0; i < 8; ++i)
    cputotal += cputime[i];

  for (i = 0; i < proccount; ++i)
  {
    if (procs[i]->stat[0] == 'R')
      ++rcount;
    else if (procs[i]->stat[0] == 'S' || procs[i]->stat[0] == 'I')
      ++slcount;
    else if (procs[i]->stat[0] == 'T')
      ++stcount;
    else if (procs[i]->stat[0] == 'Z')
      ++zcount;
  }

  move(0, 0); // 왼쪽 상단으로 이동
  clrtoeol(); // 한줄 지움
  printw("top - %s up %s,  %d users,  load average: %.2f, %.2f, %.2f",
         nowstr,
         upstr,
         usercount,
         loadaverages[0],
         loadaverages[1],
         loadaverages[2]);
  nextline();

  printw("Tasks: %d total,   %d running, %d sleeping,   %d stopped,   %d zombie", proccount, rcount, slcount, stcount, zcount);
  nextline();

  printw("%%Cpu(s):  %.1lf us,  %.1lf sy,  %.1lf ni, %.1lf id,  %.1lf wa,  %.1lf hi,  %.1lf si,  %.1lf st",
         (cputime[0] / cputotal) * 100,
         (cputime[1] / cputotal) * 100,
         (cputime[2] / cputotal) * 100,
         (cputime[3] / cputotal) * 100,
         (cputime[4] / cputotal) * 100,
         (cputime[5] / cputotal) * 100,
         (cputime[6] / cputotal) * 100,
         (cputime[7] / cputotal) * 100);
  nextline();

  printw("MiB Mem :  %.1lf total,    %.1lf free,   %.1lf used,  %.1lf buff/cache",
         memtotal / 1049., memfree / 1049., (memtotal - memavail) / 1049., (membuf + memcache) / 1049.);
  nextline();

  printw("MiB Swap:   %.1lf total,   %.1lf free,     %.1lf used.  %.1lf avail Mem",
         swaptotal / 1049., swapfree / 1049., (swaptotal - swapfree) / 1049., memavail / 1049.);
  nextline();
}

void draw_table()
{
  int i;
  int maxrows;
  int x;
  proc_t *cur;

  maxrows = h - 7;

  clrtoeol();
  attroff(A_STANDOUT);
  attron(A_BLINK);
  hline(' ', w);
  printw("%8s %-8s %4s %3s %7s %7s %7s %c %5s %5s %9s %s",
         "PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", 'S', "%CPU", "%MEM", "TIME+", "COMMAND");
  nextline();
  attroff(A_BLINK);
  attron(A_STANDOUT);

  // 범위 안넘어가게 만듦
  if (offset < 0)
    offset = 0;
  else if (proccount < offset + maxrows)
    offset = proccount - maxrows;

  for (i = 0; i < maxrows; i++)
  {
    hline(' ', w);
    cur = procs[offset + i];
    printw("%8d %-8s %4ld %3ld %7ld %7ld %7ld %c %5.1f %5.1f %9s %s",
           cur->pid,
           cur->username,
           cur->priority,
           cur->nice,
           cur->vsize,
           cur->rss,
           cur->shr,
           cur->stat[0],
           cur->cpu,
           cur->mem,
           cur->time,
           cur->command);
    nextline();
  }
}

void handler(int sig)
{
  // endwin 안하면 터미널 이상해짐
  endwin();
  exit(0);
}

void nextline()
{
  int y, x;
  getyx(stdscr, y, x);
  move(y + 1, 0);
}

void readproc()
{
  unsigned long utime;
  unsigned long stime;
  unsigned long starttime;
  DIR *dir;
  struct dirent *entry;
  char *pid;
  FILE *fp;
  char tmp[1024];
  struct stat st;
  proc_t *cur;
  struct passwd *pws;
  int i;

  getmeminfo(tmp, "MemTotal");
  sscanf(tmp, "%ld", &memtotal);

  getmeminfo(tmp, "MemFree");
  sscanf(tmp, "%ld", &memfree);

  getmeminfo(tmp, "MemAvailable");
  sscanf(tmp, "%ld", &memavail);

  getmeminfo(tmp, "Buffers");
  sscanf(tmp, "%ld", &membuf);

  getmeminfo(tmp, "Cached");
  sscanf(tmp, "%ld", &memcache);

  getmeminfo(tmp, "SwapTotal");
  sscanf(tmp, "%ld", &swaptotal);

  getmeminfo(tmp, "SwapFree");
  sscanf(tmp, "%ld", &swapfree);

  proccount = 0;

  // 프로세스 목록을 가져옴
  dir = opendir("/proc");
  while ((entry = readdir(dir)) != NULL)
  {
    // 숫자가 아닐 경우는 무시함
    if (!isdigitstr(entry->d_name))
      continue;
    pid = entry->d_name;

    procs[proccount] = (proc_t *)malloc(sizeof(proc_t));
    cur = procs[proccount];

    // stat 엶
    sprintf(tmp, "/proc/%s/stat", pid);

    stat(tmp, &st);
    fp = fopen(tmp, "r");

    if (fp == NULL)
      continue;

    // username얻기
    if ((pws = getpwuid(st.st_uid)) != NULL)
      strcpy(cur->username, pws->pw_name);
    else
      sprintf(cur->username, "%d", st.st_uid);
    // 3번째 필드까지 읽음
    fscanf(fp, "%d (%[^)]%*c%s", &cur->pid, cur->command, cur->stat);

    // (14) utime, (15)stime 읽음
    for (i = 0; i < 11; i++)
      fscanf(fp, "%ld", &utime);
    fscanf(fp, "%ld", &stime);

    // (18)pr, (19)ni 읽음
    for (i = 0; i < 3; i++)
      fscanf(fp, "%ld", &cur->priority);
    fscanf(fp, "%ld", &cur->nice);

    // (22) starttime 읽음
    for (i = 0; i < 3; i++)
      fscanf(fp, "%ld", &starttime);

    fclose(fp);

    // 메모리 사용량 읽음
    sprintf(tmp, "/proc/%s/statm", pid);
    fp = fopen(tmp, "r");
    fread(tmp, 1, 1024, fp);
    fclose(fp);
    sscanf(tmp, "%ld %ld %ld", &cur->vsize, &cur->rss, &cur->shr);

    // %CPU 계산
    cur->cpu =
        floor(cpupercent(uptime(), utime, stime, starttime) * 10) / 10;
    // %MEM 계산
    cur->mem =
        floor((double)cur->rss / memtotal * 100 * 10) / 10;

    // 실행시간 계산
    {
      long secs = uptime() - (starttime / (sysconf(_SC_CLK_TCK)));
      int mins = secs / 60;
      sprintf(cur->time, "%2d:%2ld", mins % 60, secs % 60);
    }

    proccount++;
  }
}

void freeproc()
{
  int i;
  for (i = 0; i < proccount; ++i)
    free(procs[i]);
}

bool isdigitstr(const char *str)
{
  int i;
  for (i = 0; str[i]; i++)
    if (!isdigit(str[i]))
      return false;
  return true;
}

double cpupercent(time_t uptime, unsigned long utime, unsigned long stime, unsigned long starttime)
{
  unsigned long total_time = (utime + stime) / sysconf(_SC_CLK_TCK);   // CPU사용 시간
  unsigned long seconds = uptime - (starttime / sysconf(_SC_CLK_TCK)); // 프로세스가 실행된 이후 시간
  if (seconds > 0)
    return ((double)total_time / seconds) * 100;
  else
    return 0.;
}

time_t uptime()
{
  FILE *fp;
  char tmp[1024];
  time_t result;

  fp = fopen("/proc/uptime", "r");
  fread(tmp, 1, 1024, fp);
  fclose(fp);
  sscanf(tmp, "%ld", &result);
  return result;
}

void getmeminfo(char *result, const char *field)
{
  char name[1024];
  char value[1024];
  FILE *fp;
  size_t read;
  char *line = NULL;
  size_t len = 0;
  int i;

  bzero(result, strlen(result));
  fp = fopen("/proc/meminfo", "r");
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