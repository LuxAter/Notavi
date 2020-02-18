#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define NOTAVI_STRINGIFY_IMPL(s) #s
#define NOTAVI_STRINGIFY(s) NOTAVI_STRINGIFY_IMPL(s)

#define NOTAVI_VERSION_MAJOR 0
#define NOTAVI_VERSION_MINOR 0
#define NOTAVI_VERSION_PATCH 1
#define NOTAVI_VERSION                                                         \
  NOTAVI_STRINGIFY(NOTAVI_VERSION_MAJOR)                                       \
  "." NOTAVI_STRINGIFY(NOTAVI_VERSION_MINOR) "." NOTAVI_STRINGIFY(             \
      NOTAVI_VERSION_PATCH)

#define CTRL_KEY(k) ((k)&0x1f)

enum EditorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DELETE_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
};

typedef struct erow {
  int size;
  char *chars;
} erow;

struct EditorConfig {
  int screen_rows, screen_cols;
  int cx, cy;
  int numrows;
  erow row;
  struct termios original_termios;
};

static struct EditorConfig Editor;

void die(const char *msg) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(msg);
  exit(1);
}

void disable_raw_mode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.original_termios) == -1)
    die("tcsetattr");
}

void alternate_screen_buffer() {
  write(STDOUT_FILENO, "\x1b[?1049h", 8);
}
void primary_screen_buffer() {
  write(STDOUT_FILENO, "\x1b[?1049l", 8);
}

void enable_raw_mode() {
  alternate_screen_buffer();
  if (tcgetattr(STDIN_FILENO, &Editor.original_termios) == -1)
    die("tcgetattr");
  atexit(disable_raw_mode);
  atexit(primary_screen_buffer);
  struct termios raw = Editor.original_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int editor_read_key() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] < '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DELETE_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

bool get_cursor_position(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return false;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    ++i;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return false;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return false;
  return true;
}

bool get_window_size(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return false;
    return get_cursor_position(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return true;
  }
}

void editor_open() {
  char *line = "Hello, world!";
  ssize_t linelen = 13;
  Editor.row.size = linelen;
  Editor.row.chars = malloc(linelen + 1);
  memcpy(Editor.row.chars, line, linelen);
  Editor.row.chars[linelen] = '\0';
  Editor.numrows = 1;
}

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }

void ab_append(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void ab_free(struct abuf *ab) { free(ab->b); }

void editor_draw_rows(struct abuf *ab) {
  int y;
  for (y = 0; y < Editor.screen_rows; ++y) {
    if (y == Editor.screen_rows / 3) {
      char welcome[80];
      int welcome_len = snprintf(welcome, sizeof(welcome),
                                 "Notavi Editor -- Version %s", NOTAVI_VERSION);
      if (welcome_len > Editor.screen_cols)
        welcome_len = Editor.screen_cols;
      int padding = (Editor.screen_cols - welcome_len) / 2;
      if (padding) {
        ab_append(ab, "~", 1);
        padding--;
      }
      while (padding--)
        ab_append(ab, " ", 1);
      ab_append(ab, welcome, welcome_len);
    } else {
      ab_append(ab, "~", 1);
    }
    ab_append(ab, "\x1b[K", 3);
    if (y < Editor.screen_rows - 1) {
      ab_append(ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen() {
  struct abuf ab = ABUF_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", Editor.cy + 1, Editor.cx + 1);
  ab_append(&ab, buf, strlen(buf));

  ab_append(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  ab_free(&ab);
}

void editor_move_cursor(int key) {
  switch (key) {
  case ARROW_LEFT:
    if (Editor.cx != 0)
      Editor.cx--;
    break;
  case ARROW_RIGHT:
    if (Editor.cx != Editor.screen_cols - 1)
      Editor.cx++;
    break;
  case ARROW_UP:
    if (Editor.cy != 0)
      Editor.cy--;
    break;
  case ARROW_DOWN:
    if (Editor.cy != Editor.screen_rows - 1)
      Editor.cy++;
    break;
  }
}

void editor_process_keypress() {
  int c = editor_read_key();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case HOME_KEY:
    Editor.cx = 0;
    break;
  case END_KEY:
    Editor.cx = Editor.screen_cols - 1;
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    int times = Editor.screen_rows;
    while (times--) {
      editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
  }
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editor_move_cursor(c);
    break;
  }
}

void init_editor() {
  Editor.cx = 0;
  Editor.cy = 0;
  Editor.numrows = 0;
  if (get_window_size(&Editor.screen_rows, &Editor.screen_cols) == false)
    die("get_window_size");
}

int main(int argc, char *argv[]) {
  enable_raw_mode();
  init_editor();

  editor_open();

  while (true) {
    editor_refresh_screen();
    editor_process_keypress();
  }
  return 0;
}
