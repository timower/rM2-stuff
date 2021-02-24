/* See LICENSE for licence details. */
/* yaft.c: include main function */

#include "yaft.h"

#include "conf.h"
#include "util.h"

#include "fb/common.h"
#include "terminal.h"

#include "parse.h"

#include "keyboard.h"

#include <iostream>

#include <linux/kd.h>
#include <linux/vt.h>

static const char* term_name = "yaft-256color";
static const char* shell_cmd = "/bin/bash";

struct termios termios_orig;
volatile sig_atomic_t vt_active = true;    /* SIGUSR1: vt is active or not */
volatile sig_atomic_t need_redraw = false; /* SIGUSR1: vt activated */
volatile sig_atomic_t child_alive =
  false; /* SIGCHLD: child process (shell) is alive or not */

void
sig_handler(int signo) {
  sigset_t sigset;
  /* global */
  extern volatile sig_atomic_t vt_active;
  extern volatile sig_atomic_t child_alive;
  extern volatile sig_atomic_t need_redraw;

  logging(DEBUG, "caught signal! no:%d\n", signo);

  if (signo == SIGCHLD) {
    child_alive = false;
    wait(NULL);
  }

  if constexpr (VT_CONTROL) {
    if (signo == SIGUSR1) { /* vt activate */
      vt_active = true;
      need_redraw = true;
      ioctl(STDIN_FILENO, VT_RELDISP, VT_ACKACQ);
    } else if (signo == SIGUSR2) { /* vt deactivate */
      vt_active = false;
      ioctl(STDIN_FILENO, VT_RELDISP, 1);

      if (BACKGROUND_DRAW) { /* update passive cursor */
        need_redraw = true;
      } else { /* sleep until next vt switching */
        sigfillset(&sigset);
        sigdelset(&sigset, SIGUSR1);
        sigsuspend(&sigset);
      }
    }
  }
}

void
set_rawmode(int fd, struct termios* save_tm) {
  struct termios tm;

  tm = *save_tm;
  tm.c_iflag = tm.c_oflag = 0;
  tm.c_cflag &= ~CSIZE;
  tm.c_cflag |= CS8;
  tm.c_lflag &= ~(ECHO | ISIG | ICANON);
  tm.c_cc[VMIN] = 1;  /* min data size (byte) */
  tm.c_cc[VTIME] = 0; /* time out */
  etcsetattr(fd, TCSAFLUSH, &tm);
}

bool
tty_init(struct termios* termios_orig) {
  struct sigaction sigact;

  memset(&sigact, 0, sizeof(struct sigaction));
  sigact.sa_handler = sig_handler;
  sigact.sa_flags = SA_RESTART;
  esigaction(SIGCHLD, &sigact, NULL);

  if (USE_STDIN) {
    if (VT_CONTROL) {
      esigaction(SIGUSR1, &sigact, NULL);
      esigaction(SIGUSR2, &sigact, NULL);

      struct vt_mode vtm;
      vtm.mode = VT_PROCESS;
      vtm.waitv = 0;
      vtm.acqsig = SIGUSR1;
      vtm.relsig = SIGUSR2;
      vtm.frsig = 0;

      if (ioctl(STDIN_FILENO, VT_SETMODE, &vtm)) {
        logging(WARN,
                "ioctl: VT_SETMODE failed (maybe here is not console)\n ");
      }

      if (FORCE_TEXT_MODE == false) {
        if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS)) {
          logging(WARN,
                  "ioctl: KDSETMODE failed (maybe here is not console)\n ");
        }
      }
    }

    etcgetattr(STDIN_FILENO, termios_orig);

    set_rawmode(STDIN_FILENO, termios_orig);

    ewrite(STDIN_FILENO, "\033[?25l", 6); /* make cusor invisible */
  }

  return true;
}

void
tty_die(struct termios* termios_orig) {
  /* no error handling */
  struct sigaction sigact;
  struct vt_mode vtm;

  memset(&sigact, 0, sizeof(struct sigaction));
  sigact.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &sigact, NULL);

  if (USE_STDIN) {
    if (VT_CONTROL) {
      sigaction(SIGUSR1, &sigact, NULL);
      sigaction(SIGUSR2, &sigact, NULL);

      vtm.mode = VT_AUTO;
      vtm.waitv = 0;
      vtm.relsig = vtm.acqsig = vtm.frsig = 0;

      ioctl(STDIN_FILENO, VT_SETMODE, &vtm);

      if (FORCE_TEXT_MODE == false)
        ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT);
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, termios_orig);
    fflush(stdout);
    ewrite(STDIN_FILENO, "\033[?25h", 6); /* make cursor visible */
  }
}

bool
fork_and_exec(int* master,
              const char* cmd,
              const char* const argv[],
              int lines,
              int cols) {
  pid_t pid;
  struct winsize ws;
  ws.ws_row = lines;
  ws.ws_col = cols;
  /* XXX: this variables are UNUSED (man tty_ioctl),
          but useful for calculating terminal cell size */
  ws.ws_ypixel = CELL_HEIGHT * lines;
  ws.ws_xpixel = CELL_WIDTH * cols;

  pid = eforkpty(master, NULL, NULL, &ws);
  if (pid < 0)
    return false;
  else if (pid == 0) { /* child */
    esetenv("TERM", term_name, 1);
    eexecvp(cmd, argv);
    /* never reach here */
    exit(EXIT_FAILURE);
  }
  return true;
}

constexpr auto select_timeout = std::chrono::microseconds(SELECT_TIMEOUT);

int
main(int argc, const char* argv[]) {
  extern const char* shell_cmd; /* defined in conf.h */
  const char* cmd;
  const char** args;
  uint8_t buf[BUFSIZE];
  ssize_t size;
  struct terminal_t term;
  /* global */
  extern volatile sig_atomic_t need_redraw;
  extern volatile sig_atomic_t child_alive;
  extern struct termios termios_orig;
  static const char* shell_args[2] = { shell_cmd, NULL };

  Keyboard keyboard;

  /* init */
  if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */
    logging(WARN, "setlocale falied\n");

  auto fb = rmlib::fb::FrameBuffer::open();
  if (fb.isError()) {
    logging(FATAL, "framebuffer initialize failed\n");
    goto fb_init_failed;
  }

  if (!term_init(&term, fb->canvas.width, fb->canvas.height)) {
    logging(FATAL, "terminal initialize failed\n");
    goto term_init_failed;
  }

  if (!tty_init(&termios_orig)) {
    logging(FATAL, "tty initialize failed\n");
    goto tty_init_failed;
  }

  /* fork and exec shell */
  if (argc > 1) {
    cmd = argv[1];
    args = argv + 1;
  } else {
    cmd = shell_cmd;
    args = shell_args;
  }

  if (!fork_and_exec(&term.fd, cmd, args, term.lines, term.cols)) {
    logging(FATAL, "forkpty failed\n");
    goto tty_init_failed;
  }
  child_alive = true;

  if (keyboard.init(*fb, term).isError()) {
    logging(FATAL, "Keyboard failed\n");
    goto tty_init_failed;
  }

  keyboard.draw();

  /* main loop */
  while (child_alive) {
    if (need_redraw) {
      need_redraw = false;
      redraw(&term);
      refresh(*fb, &term);
    }

    auto eventAndFds = [&] {
      if constexpr (USE_STDIN) {
        return keyboard.input.waitForInput(
          select_timeout, term.fd, STDIN_FILENO);
      } else {
        return keyboard.input.waitForInput(select_timeout, term.fd);
      }
    }();

    // Update repeat in any case (timeout, error or events).
    keyboard.updateRepeat();

    if (eventAndFds.isError()) {
      std::cerr << "Error reading input: " << eventAndFds.getError().msg
                << "\n";
      continue;
    }

    const auto& [events, fds] = *eventAndFds;
    keyboard.handleEvents(events);

    if (fds[0]) {
      if ((size = read(term.fd, buf, BUFSIZE)) > 0) {
        if (VERBOSE)
          ewrite(STDOUT_FILENO, buf, size);
        parse(&term, buf, size);
        if (LAZY_DRAW && size == BUFSIZE)
          continue; /* maybe more data arrives soon */
        refresh(*fb, &term);
      }
    }

    if constexpr (USE_STDIN) {
      if (fds[1]) {
        if ((size = read(STDIN_FILENO, buf, BUFSIZE)) > 0)
          ewrite(term.fd, buf, size);
      }
    }
  }

  /* normal exit */
  tty_die(&termios_orig);
  term_die(&term);
  return EXIT_SUCCESS;

  /* error exit */
tty_init_failed:
  term_die(&term);
term_init_failed:
fb_init_failed:
  return EXIT_FAILURE;
}
