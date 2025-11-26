// FogOSv2 Project 2 – Minimal Shell (xv6 style)
// Author: Yifan Wan
//
// Features:
//   • run external programs (fork + exec + wait)
//   • built-ins: cd, exit
//   • input/output redirection (<, >)
//   • single pipeline (a | b)
//   • background job (&)
//   • script mode: myshell script.txt
//
// Works under FogOSv2/xv6 userland (no libc, only user.h syscalls)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXARGS 16
#define MAXLINE 256

// read a line from fd into buf (no fgets in xv6)
int readline(int fd, char *buf, int n) {
  int i = 0;
  while (i + 1 < n) {
    char c;
    int cc = read(fd, &c, 1);
    if (cc < 1) break;
    if (c == '\n' || c == '\r') break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return i;
}

// simple splitter by spaces
int split(char *buf, char **argv, int max) {
  int argc = 0;
  while (*buf && argc < max - 1) {
    while (*buf == ' ' || *buf == '\t') buf++;
    if (*buf == 0) break;
    argv[argc++] = buf;
    while (*buf && *buf != ' ' && *buf != '\t') buf++;
    if (*buf) *buf++ = 0;
  }
  argv[argc] = 0;
  return argc;
}

void runcmd(char *line);

void runpipeline(char *cmd1[], char *cmd2[]) {
  int p[2];
  pipe(p);
  if (fork() == 0) {
    close(1);
    dup(p[1]);
    close(p[0]);
    close(p[1]);
    exec(cmd1[0], cmd1);
    printf("exec %s failed\n", cmd1[0]);
    exit(1);
  }
  if (fork() == 0) {
    close(0);
    dup(p[0]);
    close(p[0]);
    close(p[1]);
    exec(cmd2[0], cmd2);
    printf("exec %s failed\n", cmd2[0]);
    exit(1);
  }
  close(p[0]);
  close(p[1]);
  wait(0);
  wait(0);
}

void runcmd(char *line) {
  char *argv[MAXARGS];
  int argc = split(line, argv, MAXARGS);
  if (argc == 0) return;

  // built-ins
  if (strcmp(argv[0], "exit") == 0) {
    exit(0);
  }
  if (strcmp(argv[0], "cd") == 0) {
    if (argc < 2) printf("cd: missing path\n");
    else if (chdir(argv[1]) < 0)
      printf("cd: cannot cd %s\n", argv[1]);
    return;
  }

  // detect &
  int background = 0;
  if (strcmp(argv[argc - 1], "&") == 0) {
    background = 1;
    argv[argc - 1] = 0;
  }

  // detect pipe
  int pipepos = -1;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "|") == 0) { pipepos = i; break; }
  }
  if (pipepos != -1) {
    argv[pipepos] = 0;
    runpipeline(argv, &argv[pipepos + 1]);
    return;
  }

  // redirections
  char *infile = 0, *outfile = 0;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "<") == 0 && i + 1 < argc) {
      infile = argv[i + 1];
      argv[i] = 0;
      break;
    }
    if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
      outfile = argv[i + 1];
      argv[i] = 0;
      break;
    }
  }

  if (fork() == 0) {
    if (infile) {
      int fd = open(infile, O_RDONLY);
      if (fd < 0) { printf("open %s failed\n", infile); exit(1); }
      close(0);
      dup(fd);
      close(fd);
    }
    if (outfile) {
      int fd = open(outfile, O_WRONLY | O_CREATE | O_TRUNC);
      if (fd < 0) { printf("open %s failed\n", outfile); exit(1); }
      close(1);
      dup(fd);
      close(fd);
    }
    exec(argv[0], argv);
    printf("exec %s failed\n", argv[0]);
    exit(1);
  }

  if (!background)
    wait(0);
}

void repl(int fd) {
  char line[MAXLINE];
  while (1) {
    if (fd == 0) { // interactive
      printf("$ ");
    }
    int n = readline(fd, line, MAXLINE);
    if (n <= 0) break;
    runcmd(line);
  }
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
      printf("cannot open %s\n", argv[1]);
      exit(1);
    }
    repl(fd);
    close(fd);
  } else {
    repl(0);
  }
  exit(0);
}
