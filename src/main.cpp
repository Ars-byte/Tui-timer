#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "ui.hpp"

namespace {

volatile std::sig_atomic_t g_quit = 0;
void on_signal(int) { g_quit = 1; }

// Terminal en modo "raw": pantalla alternativa, cursor oculto y teclas sin
// eco. Restaura todo al salir (RAII).
class Terminal {
 public:
  Terminal() {
    if (isatty(STDIN_FILENO)) {
      ::tcgetattr(STDIN_FILENO, &old_);
      struct termios raw = old_;
      raw.c_lflag &= ~(ICANON | ECHO | ISIG);
      raw.c_iflag &= ~(IXON | ICRNL);
      raw.c_cc[VMIN] = 0;
      raw.c_cc[VTIME] = 0;
      ::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    std::cout << "\x1b[?1049h\x1b[?25l" << std::flush;
  }
  ~Terminal() {
    std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l" << std::flush;
    ::tcsetattr(STDIN_FILENO, TCSANOW, &old_);
  }
  int width() const {
    struct winsize w {};
    ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col ? w.ws_col : 80;
  }
  int height() const {
    struct winsize w {};
    ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row ? w.ws_row : 24;
  }

 private:
  struct termios old_ {};
};

// Lee una tecla sin bloquear. Devuelve -1 si no hay nada en timeout_ms.
int read_key(int timeout_ms) {
  struct pollfd p {STDIN_FILENO, POLLIN, 0};
  if (::poll(&p, 1, timeout_ms) <= 0) return -1;
  unsigned char c = 0;
  if (::read(STDIN_FILENO, &c, 1) != 1) return -1;
  return static_cast<int>(c);
}

// Acepta: "5:30", "90" (segundos), "2m", "1h", "1:30.5", "0:45"
bool parse_duration(const std::string& in, double& out) {
  std::string t;
  for (char c : in)
    if (c != ' ') t += c;
  if (t.empty()) return false;

  double mult = 1.0;
  char last = t.back();
  if (last == 'm' || last == 'M') {
    mult = 60.0;
    t.pop_back();
  } else if (last == 'h' || last == 'H') {
    mult = 3600.0;
    t.pop_back();
  } else if (last == 's' || last == 'S') {
    t.pop_back();
  }
  if (t.empty()) return false;

  double total = 0.0;
  int parts = 0;
  std::string seg;
  bool any_digit = false;
  for (char c : t) {
    if (c >= '0' && c <= '9') {
      seg += c;
      any_digit = true;
    } else if (c == '.') {
      seg += c;
    } else if (c == ':') {
      if (parts >= 2) return false;
      total = total * 60.0 + std::atof(seg.c_str());
      seg.clear();
      ++parts;
    } else {
      return false;
    }
  }
  total = total * 60.0 + std::atof(seg.c_str());
  if (!any_digit || total <= 0.0 || total > 24.0 * 3600.0) return false;
  out = total * mult;
  return out > 0.0;
}

struct App {
  tui::Mode mode = tui::Mode::COUNTDOWN;
  tui::State state = tui::State::IDLE;
  double total = 300.0;  // 5:00 por defecto
  double elapsed_base = 0.0;
  bool running = false;
  std::chrono::steady_clock::time_point last_start;
  std::chrono::steady_clock::time_point last_alarm;
  std::string input;
  bool input_error = false;

  double elapsed() const {
    double base = elapsed_base;
    if (running)
      base += std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - last_start)
                  .count();
    return base;
  }
  double remaining() const { return total - elapsed(); }

  void toggle_run() {
    auto now = std::chrono::steady_clock::now();
    switch (state) {
      case tui::State::IDLE:
        running = true;
        last_start = now;
        state = tui::State::RUNNING;
        break;
      case tui::State::RUNNING:
        elapsed_base = elapsed();
        running = false;
        state = tui::State::PAUSED;
        break;
      case tui::State::PAUSED:
        running = true;
        last_start = now;
        state = tui::State::RUNNING;
        break;
      case tui::State::FINISHED:
        elapsed_base = 0.0;
        running = false;
        state = tui::State::IDLE;
        break;
      case tui::State::SW_IDLE:
        running = true;
        last_start = now;
        state = tui::State::SW_RUN;
        break;
      case tui::State::SW_RUN:
        elapsed_base = elapsed();
        running = false;
        state = tui::State::SW_PAUSED;
        break;
      case tui::State::SW_PAUSED:
        running = true;
        last_start = now;
        state = tui::State::SW_RUN;
        break;
      case tui::State::SETTING:
        break;
    }
  }

  void reset() {
    running = false;
    elapsed_base = 0.0;
    state = (mode == tui::Mode::COUNTDOWN) ? tui::State::IDLE
                                           : tui::State::SW_IDLE;
  }

  void switch_mode() {
    mode = (mode == tui::Mode::COUNTDOWN) ? tui::Mode::STOPWATCH
                                          : tui::Mode::COUNTDOWN;
    reset();
  }
};

// Devuelve false cuando hay que salir.
bool handle_key(App& a, int k) {
  if (k == 'q' || k == 'Q' || k == 3 /*Ctrl+C*/ || k == 4 /*Ctrl+D*/)
    return false;

  if (a.state == tui::State::SETTING) {
    if (k == 27) {  // ESC: cancelar
      a.input.clear();
      a.input_error = false;
      a.state = tui::State::IDLE;
    } else if (k == '\n' || k == '\r') {  // ENTER: aceptar
      double sec;
      if (parse_duration(a.input, sec)) {
        a.total = sec;
        a.elapsed_base = 0.0;
        a.running = false;
        a.state = tui::State::IDLE;
      } else {
        a.input_error = true;
      }
    } else if (k == 127 || k == 8) {  // retroceso
      if (!a.input.empty()) a.input.pop_back();
      a.input_error = false;
    } else if ((k >= '0' && k <= '9') || k == ':' || k == '.') {
      if (a.input.size() < 12) a.input += static_cast<char>(k);
      a.input_error = false;
    }
    return true;
  }

  switch (k) {
    case ' ':
    case 'p':
    case 'P':
      a.toggle_run();
      break;
    case 'n':
    case 'N':
      if (a.mode == tui::Mode::COUNTDOWN) {
        a.running = false;
        a.input.clear();
        a.input_error = false;
        a.state = tui::State::SETTING;
      }
      break;
    case 's':
    case 'S':
      a.reset();
      break;
    case 'm':
    case 'M':
      a.switch_mode();
      break;
    default:
      break;
  }
  return true;
}

void update(App& a) {
  auto now = std::chrono::steady_clock::now();
  if (a.running && a.mode == tui::Mode::COUNTDOWN && a.remaining() <= 0.0) {
    a.running = false;
    a.elapsed_base = a.total;
    a.state = tui::State::FINISHED;
    a.last_alarm = now;
    std::cout << '\a' << std::flush;  // pitido
  }
  // Recordatorio cada 2 s mientras esté terminado
  if (a.state == tui::State::FINISHED &&
      now - a.last_alarm > std::chrono::seconds(2)) {
    a.last_alarm = now;
    std::cout << '\a' << std::flush;
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
  std::signal(SIGPIPE, SIG_IGN);

  Terminal term;
  App app;

  // Duración opcional en línea de comandos: tui-timer 25:00
  if (argc > 1) {
    double sec;
    if (parse_duration(argv[1], sec)) app.total = sec;
  }

  while (!g_quit) {
    int k = read_key(50);  // 20 fps
    if (k >= 0 && !handle_key(app, k)) break;
    update(app);

    tui::RenderInfo info;
    info.mode = app.mode;
    info.state = app.state;
    info.input = app.input;
    info.input_error = app.input_error;
    if (app.mode == tui::Mode::COUNTDOWN) {
      info.total = app.total;
      info.remaining = app.remaining();
    } else {
      info.elapsed = app.elapsed();
    }
    tui::render(info, term.width(), term.height());
  }

  return 0;
}
