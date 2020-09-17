#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// 최대 입력 크기
#define MAX_INPUT_SIZE 1024
// 최대 토큰 크기
#define MAX_TOKEN_SIZE 64
// 최대 토큰 개수
#define MAX_NUM_TOKENS 64

void do_line(char **tokens);
// 프로세스 생성하는 함수
void spawn(int argc, char **argv, int infd, int outfd);

typedef struct proc
{
  pid_t pid;
  int pipefd[2];
} proc_t;

// 공백과 개행 및 탭으로 토큰 분리
// line: 읽어들인 행을 받음
/* Splits the string by space and returns the array of tokens
*
*/
char **tokenize(char *line)
{
  // 공간 할당
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  int i, tokenIndex = 0, tokenNo = 0;

  // 한글자씩 순회
  for (i = 0; i < strlen(line); i++)
  {

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t')
    {
      // 공백문자일 경우, 토큰문자의 끝을 NULL로 채우고 tokens에 추가함.
      token[tokenIndex] = '\0';
      if (tokenIndex != 0)
      {
        tokens[tokenNo] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
        strcpy(tokens[tokenNo++], token);
        tokenIndex = 0;
      }
    }
    else
    {
      // 토큰을 채움
      token[tokenIndex++] = readChar;
    }
  }

  // 공간 해제 후 토큰목록 리턴
  free(token);
  tokens[tokenNo] = NULL;
  return tokens;
}

int main(int argc, char *argv[])
{
  char line[MAX_INPUT_SIZE];
  char **tokens;
  int i;

  FILE *fp;
  // 인자로 파일이름이 들어왔다면
  if (argc == 2)
  {
    // 파일 오픈
    fp = fopen(argv[1], "r");
    if (fp == NULL)
    {
      printf("File doesn't exists.");
      return -1;
    }
  }

  while (1)
  {
    /* BEGIN: TAKING INPUT */
    bzero(line, sizeof(line));
    if (argc == 2) // 배치파일 모드라면 한줄 읽어서 line 초기화
    {              // batch mode
      if (fgets(line, sizeof(line), fp) == NULL)
      { // file reading finished
        break;
      }
      if (line[strlen(line) - 1] == '\n')
        line[strlen(line) - 1] = '\0';
    }
    else // 대화식 모드
    {    // interactive mode
      printf("$ ");
      scanf("%[^\n]", line);
      // EOF입력시 쉘 종료
      if (getchar() == EOF)
      {
        putchar('\n');
        exit(EXIT_SUCCESS);
      }
    }
#ifdef DEBUG
    // 입력된 명령어 출력하여 확인
    printf("Command entered: %s (remove this debug output later)\n", line);
#endif
    /* END: TAKING INPUT */

    // 문자열 끝을 \n으로 통일하고 토큰을 초기화
    line[strlen(line)] = '\n'; //terminate with new line
    tokens = tokenize(line);

    //do whatever you want with the commands, here we just print them
#ifdef DEBUG
    // 초기화된 토큰을 확인
    for (i = 0; tokens[i] != NULL; i++)
    {
      printf("found token %s (remove this debug output later)\n", tokens[i]);
    }
#endif
    if (tokens[0] != NULL)
      do_line(tokens);

    // 토큰 해제
    // Freeing the allocated memory
    for (i = 0; tokens[i] != NULL; i++)
      free(tokens[i]);
    free(tokens);
  }
  return 0;
}

void do_line(char **tokens)
{
  int i;
  proc_t **procs;
  int procNum;
  int procIndex;
  int argc;
  char **argv;

  procNum = 1;
  for (i = 0; tokens[i] != NULL; i++)
    if (strcmp(tokens[i], "|") == 0)
      procNum++;
  procs = (proc_t **)malloc(sizeof(proc_t *) * procNum);
  for (i = 0; i < procNum; ++i)
  {
    procs[i] = (proc_t *)malloc(sizeof(proc_t));
    procs[i]->pipefd[0] = -1;
    procs[i]->pipefd[1] = -1;
  }

  argv = tokens;
  argc = 0;
  procIndex = 0;
  for (i = 0; tokens[i] != NULL; i++)
  {
    if (strcmp(tokens[i], "|") == 0)
    {
      if (pipe(procs[procIndex]->pipefd) == -1)
        perror("pipe");
      free(tokens[i]);
      tokens[i] = NULL;
      spawn(argc, argv, procIndex == 0 ? -1 : procs[procIndex - 1]->pipefd[0], procs[procIndex]->pipefd[1]);
      argv = tokens + i + 1;
      argc = 0;
      procIndex++;
      continue;
    }
    argc++;
  }
  spawn(argc, argv, procIndex == 0 ? -1 : procs[procIndex - 1]->pipefd[0], -1);

  for (i = 0; i < procNum; ++i)
    free(procs[i]);
  free(procs);
}

// 프로세스 생성하는 함수
void spawn(int argc, char **argv, int infd, int outfd)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0)
  {
    // child
    if (infd != -1)
      dup2(infd, STDIN_FILENO);
    if (outfd != -1)
      dup2(outfd, STDOUT_FILENO);
    if (execvp(argv[0], argv) == -1)
      fprintf(stderr, "SSUShell : Incorrect command\n");
    exit(EXIT_FAILURE);
  }
  else if (pid < 0)
    perror("ssu_shell");
  else
  {
    // parent
    do
    {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    if (infd != -1)
      close(infd);
    if (outfd != -1)
      close(outfd);
  }
}