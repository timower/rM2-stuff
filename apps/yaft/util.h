#pragma once

// eforkpty(): spawn a child attached to a fresh pty. Extracted from
// vendor/libYaft/util.h -- an independent pty-spawn helper, unrelated to the
// VT engine being replaced.

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static inline int
eopen(const char* path, int flag) {
  int fd = open(path, flag);
  if (fd < 0) {
    fprintf(stderr, "couldn't open \"%s\": %s\n", path, strerror(errno));
  }
  return fd;
}

static inline int
eopenpty(int* amaster,
         int* aslave,
         const struct termios* termp,
         const struct winsize* winsize) {
  errno = 0;
  int master = posix_openpt(O_RDWR | O_NOCTTY);
  char* name = nullptr;
  if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0 ||
      (name = ptsname(master)) == nullptr) {
    fprintf(stderr, "openpty: %s\n", strerror(errno));
    return -1;
  }
  *amaster = master;
  *aslave = eopen(name, O_RDWR | O_NOCTTY);

  if (termp != nullptr) {
    tcsetattr(*aslave, TCSAFLUSH, termp);
  }
  if (winsize != nullptr) {
    ioctl(*aslave, TIOCSWINSZ, winsize);
  }

  return 0;
}

static inline pid_t
eforkpty(int* amaster,
         const struct termios* termp,
         const struct winsize* winsize) {
  int master = -1;
  int slave = -1;
  if (eopenpty(&master, &slave, termp, winsize) < 0) {
    return -1;
  }

  errno = 0;
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork: %s\n", strerror(errno));
    return pid;
  }
  if (pid == 0) { /* child */
    close(master);
    setsid();

    dup2(slave, STDIN_FILENO);
    dup2(slave, STDOUT_FILENO);
    dup2(slave, STDERR_FILENO);

    if (ioctl(slave, TIOCSCTTY, nullptr) != 0) {
      fprintf(stderr, "ioctl: TIOCSCTTY failed\n");
    }
    close(slave);
    return 0;
  }

  /* parent */
  close(slave);
  *amaster = master;
  return pid;
}
