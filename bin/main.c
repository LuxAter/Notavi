#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
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

#define NOTAVI_TAB_STOP 8
#define NOTAVI_QUIT_TIMES 3

#define CTRL_KEY(k) ((k)&0x1f)

enum EditorKey {
  BACKSPACE = 127,
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
  int rsize;
  char *chars;
  char *render;
} erow;

struct EditorConfig {
  int screen_rows, screen_cols;
  int cx, cy;
  int rx;
  int rowoff, coloff;
  int numrows;
  int dirty;
  erow *row;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios original_termios;
};

static struct EditorConfig Editor;

void editor_set_status_message(const char *fmt, ...);
void editor_refresh_screen();
char *editor_prompt(char *prompt, void (*callback)(char *, int));

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

void alternate_screen_buffer() { write(STDOUT_FILENO, "\x1b[?1049h", 8); }
void primary_screen_buffer() { write(STDOUT_FILENO, "\x1b[?1049l", 8); }

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

int editor_row_cx_to_rx(erow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; ++j) {
    if (row->chars[j] == '\t')
      rx += (NOTAVI_TAB_STOP - 1) - (rx % NOTAVI_TAB_STOP);
    rx++;
  }
  return rx;
}

int editor_row_rx_to_cx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (NOTAVI_TAB_STOP - 1) - (cur_rx % NOTAVI_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx)
      return cx;
  }
  return cx;
}

void editor_update_row(erow *row) {
  int tabs = 0;
  for (int j = 0; j < row->size; ++j) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);
  row->render = (char *)malloc(row->size + tabs * (NOTAVI_TAB_STOP - 1) + 1);

  int idx = 0;
  for (int j = 0; j < row->size; ++j) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % NOTAVI_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editor_insert_row(int at, char *s, size_t len) {
  if (at < 0 || at > Editor.numrows)
    return;

  Editor.row = (erow *)realloc(Editor.row, sizeof(erow) * (Editor.numrows + 1));
  memmove(&Editor.row[at + 1], &Editor.row[at],
          sizeof(erow) * (Editor.numrows - at));

  Editor.row[at].size = len;
  Editor.row[at].chars = (char *)malloc(len + 1);
  memcpy(Editor.row[at].chars, s, len);
  Editor.row[at].chars[len] = '\0';

  Editor.row[at].rsize = 0;
  Editor.row[at].render = NULL;
  editor_update_row(&Editor.row[at]);

  Editor.numrows++;
  Editor.dirty++;
}

void editor_free_row(erow *row) {
  free(row->render);
  free(row->chars);
}

void editor_delete_row(int at) {
  if (at < 0 || at >= Editor.numrows)
    return;
  editor_free_row(&Editor.row[at]);
  memmove(&Editor.row[at], &Editor.row[at + 1],
          sizeof(erow) * (Editor.numrows - at - 1));
  Editor.numrows--;
  Editor.dirty++;
}

void editor_row_insert_char(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = (char *)realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editor_update_row(row);
  Editor.dirty++;
}

void editor_row_append_string(erow *row, char *s, size_t len) {
  row->chars = (char *)realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editor_update_row(row);
  Editor.dirty++;
}

void editor_row_del_char(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editor_update_row(row);
  Editor.dirty++;
}

void editor_insert_char(int c) {
  if (Editor.cy == Editor.numrows) {
    editor_insert_row(Editor.numrows, "", 0);
  }
  editor_row_insert_char(&Editor.row[Editor.cy], Editor.cx, c);
  Editor.cx++;
}

void editor_insert_newline() {
  if (Editor.cx == 0) {
    editor_insert_row(Editor.cy, "", 0);
  } else {
    erow *row = &Editor.row[Editor.cy];
    editor_insert_row(Editor.cy + 1, &row->chars[Editor.cx],
                      row->size - Editor.cx);
    row = &Editor.row[Editor.cy];
    row->size = Editor.cx;
    row->chars[row->size] = '\0';
    editor_update_row(row);
  }
  Editor.cy++;
  Editor.cx = 0;
}

void editor_delete_char() {
  if (Editor.cy == Editor.numrows)
    return;
  if (Editor.cx == 0 && Editor.cy == 0)
    return;

  erow *row = &Editor.row[Editor.cy];
  if (Editor.cx > 0) {
    editor_row_del_char(row, Editor.cx - 1);
    Editor.cx--;
  } else {
    Editor.cx = Editor.row[Editor.cy - 1].size;
    editor_row_append_string(&Editor.row[Editor.cy - 1], row->chars, row->size);
    editor_delete_row(Editor.cy);
    Editor.cy--;
  }
}

char *editor_rows_to_string(int *buflen) {
  int totlen = 0;
  for (int j = 0; j < Editor.numrows; ++j) {
    totlen += Editor.row[j].size + 1;
  }
  *buflen = totlen;

  char *buf = (char *)malloc(totlen);
  char *p = buf;
  for (int j = 0; j < Editor.numrows; ++j) {
    memcpy(p, Editor.row[j].chars, Editor.row[j].size);
    p += Editor.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editor_open(char *filename) {
  free(Editor.filename);
  Editor.filename = strdup(filename);
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editor_insert_row(Editor.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  Editor.dirty = 0;
}

void editor_save() {
  if (Editor.filename == NULL) {
    Editor.filename = editor_prompt("Save as: %s", NULL);
    if (Editor.filename == NULL) {
      editor_set_status_message("Save aborted");
      return;
    }
  }
  int len;
  char *buf = editor_rows_to_string(&len);
  int fd = open(Editor.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        Editor.dirty = 0;
        editor_set_status_message("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editor_set_status_message("Can't save! I/O error: %s", strerror(errno));
}

void editor_find_callback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;
  int current = last_match;
  for (int i = 0; i < Editor.numrows; ++i) {
    current += direction;
    if (current == -1)
      current = Editor.numrows - 1;
    else if (current == Editor.numrows)
      current = 0;

    erow *row = &Editor.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      Editor.cy = current;
      Editor.cx = editor_row_rx_to_cx(row, match - row->render);
      Editor.rowoff = Editor.numrows;
      break;
    }
  }
}

void editor_find() {
  int saved_cx = Editor.cx;
  int saved_cy = Editor.cy;
  int saved_coloff = Editor.coloff;
  int saved_rowoff = Editor.rowoff;

  char *query =
      editor_prompt("Search: %s (Use ESC/Arrows/Enter)", editor_find_callback);
  if (query) {
    free(query);
  } else {
    Editor.cx = saved_cx;
    Editor.cy = saved_cy;
    Editor.coloff = saved_coloff;
    Editor.rowoff = saved_rowoff;
  }
}

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }

void ab_append(struct abuf *ab, const char *s, int len) {
  char *new_buf = (char *)realloc(ab->b, ab->len + len);

  if (new_buf == NULL)
    return;
  memcpy(&new_buf[ab->len], s, len);
  ab->b = new_buf;
  ab->len += len;
}

void ab_free(struct abuf *ab) { free(ab->b); }

void editor_scroll() {
  Editor.rx = 0;
  if (Editor.cy < Editor.numrows) {
    Editor.rx = editor_row_cx_to_rx(&Editor.row[Editor.cy], Editor.cx);
  }

  if (Editor.cy < Editor.rowoff) {
    Editor.rowoff = Editor.cy;
  }
  if (Editor.cy >= Editor.rowoff + Editor.screen_rows) {
    Editor.rowoff = Editor.cy - Editor.screen_rows + 1;
  }
  if (Editor.rx < Editor.coloff) {
    Editor.coloff = Editor.rx;
  }
  if (Editor.rx >= Editor.coloff + Editor.screen_cols) {
    Editor.coloff = Editor.rx - Editor.screen_cols + 1;
  }
}

void editor_draw_rows(struct abuf *ab) {
  int y;
  for (y = 0; y < Editor.screen_rows; ++y) {
    int filerow = y + Editor.rowoff;
    if (filerow >= Editor.numrows) {
      if (Editor.numrows == 0 && y == Editor.screen_rows / 3) {
        char welcome[80];
        int welcome_len =
            snprintf(welcome, sizeof(welcome), "Notavi Editor -- Version %s",
                     NOTAVI_VERSION);
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
    } else {
      int len = Editor.row[filerow].rsize - Editor.coloff;
      if (len < 0)
        len = 0;
      if (len > Editor.screen_cols)
        len = Editor.screen_rows;
      ab_append(ab, &Editor.row[filerow].render[Editor.coloff], len);
    }
    ab_append(ab, "\x1b[K", 3);
    ab_append(ab, "\r\n", 2);
  }
}

void editor_draw_status_bar(struct abuf *ab) {
  ab_append(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     Editor.filename ? Editor.filename : "[No Name]",
                     Editor.numrows, Editor.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", Editor.cy + 1,
                      Editor.numrows);
  if (len > Editor.screen_cols)
    len = Editor.screen_cols;
  ab_append(ab, status, len);
  while (len < Editor.screen_cols) {
    if (Editor.screen_cols - len == rlen) {
      ab_append(ab, rstatus, rlen);
      break;
    } else {
      ab_append(ab, " ", 1);
      len++;
    }
  }
  ab_append(ab, "\x1b[m", 3);
  ab_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(struct abuf *ab) {
  ab_append(ab, "\x1b[K", 3);
  int msglen = strlen(Editor.statusmsg);
  if (msglen > Editor.screen_cols)
    msglen = Editor.screen_cols;
  if (msglen && time(NULL) - Editor.statusmsg_time < 5)
    ab_append(ab, Editor.statusmsg, msglen);
}

void editor_refresh_screen() {
  editor_scroll();

  struct abuf ab = ABUF_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);
  editor_draw_status_bar(&ab);
  editor_draw_message_bar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (Editor.cy - Editor.rowoff) + 1,
           (Editor.rx - Editor.coloff) + 1);
  ab_append(&ab, buf, strlen(buf));

  ab_append(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  ab_free(&ab);
}

void editor_set_status_message(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(Editor.statusmsg, sizeof(Editor.statusmsg), fmt, ap);
  va_end(ap);
  Editor.statusmsg_time = time(NULL);
}

char *editor_prompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = (char *)malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (true) {
    editor_set_status_message(prompt, buf);
    editor_refresh_screen();

    int c = editor_read_key();
    if (c == DELETE_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editor_set_status_message("");
      if (callback)
        callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editor_set_status_message("");
        if (callback)
          callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = (char *)realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback)
      callback(buf, c);
  }
}

void editor_move_cursor(int key) {
  erow *row = (Editor.cy >= Editor.numrows) ? NULL : &Editor.row[Editor.cy];

  switch (key) {
  case ARROW_LEFT:
    if (Editor.cx != 0) {
      Editor.cx--;
    } else if (Editor.cy > 0) {
      Editor.cy--;
      Editor.cx = Editor.row[Editor.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && Editor.cx < row->size) {
      Editor.cx++;
    } else if (row && Editor.cx == row->size) {
      Editor.cy++;
      Editor.cx = 0;
    }
    break;
  case ARROW_UP:
    if (Editor.cy != 0)
      Editor.cy--;
    break;
  case ARROW_DOWN:
    if (Editor.cy < Editor.numrows)
      Editor.cy++;
    break;
  }

  row = (Editor.cy >= Editor.numrows) ? NULL : &Editor.row[Editor.cy];
  int rowlen = row ? row->size : 0;
  if (Editor.cx > rowlen) {
    Editor.cx = rowlen;
  }
}

void editor_process_keypress() {
  static int quit_times = NOTAVI_QUIT_TIMES;
  int c = editor_read_key();
  switch (c) {
  case CTRL_KEY('q'):
    if (Editor.dirty && quit_times > 0) {
      editor_set_status_message("WARNING!!! File has unsaved changes. Press "
                                "Ctrl-Q %d more times to quit.",
                                quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case CTRL_KEY('s'):
    editor_save();
    break;
  case HOME_KEY:
    Editor.cx = 0;
    break;
  case END_KEY:
    if (Editor.cy < Editor.numrows)
      Editor.cx = Editor.row[Editor.cy].size;
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      Editor.cx = Editor.rowoff;
    } else if (c == PAGE_DOWN) {
      Editor.cy = Editor.rowoff + Editor.screen_rows - 1;
      if (Editor.cy > Editor.numrows)
        Editor.cy = Editor.numrows;
    }
    int times = Editor.screen_rows;
    while (times--)
      editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    break;
  }
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editor_move_cursor(c);
    break;
  case '\r':
    editor_insert_newline();
    break;
  case CTRL_KEY('f'):
    editor_find();
    break;
  case BACKSPACE:
  case CTRL_KEY('h'):
  case DELETE_KEY:
    if (c == DELETE_KEY)
      editor_move_cursor(ARROW_RIGHT);
    editor_delete_char();
    break;
  case CTRL_KEY('l'):
  case '\x1b':
    break;

  default:
    editor_insert_char(c);
    break;
  }
  quit_times = NOTAVI_QUIT_TIMES;
}

void init_editor() {
  Editor.cx = 0;
  Editor.cy = 0;
  Editor.rx = 0;
  Editor.rowoff = 0;
  Editor.coloff = 0;
  Editor.numrows = 0;
  Editor.dirty = 0;
  Editor.row = NULL;
  Editor.filename = NULL;
  Editor.statusmsg[0] = '\0';
  Editor.statusmsg_time = 0;

  if (get_window_size(&Editor.screen_rows, &Editor.screen_cols) == false)
    die("get_window_size");
  Editor.screen_rows -= 2;
}

int main(int argc, char *argv[]) {
  enable_raw_mode();
  init_editor();

  if (argc >= 2) {
    editor_open(argv[1]);
  }

  editor_set_status_message(
      "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  while (true) {
    editor_refresh_screen();
    editor_process_keypress();
  }
  return 0;
}
