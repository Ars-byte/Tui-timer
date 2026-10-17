#include "ui.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace tui {

#define RST "\x1b[0m"
#define BOLD "\x1b[1m"
#define DIM "\x1b[2m"
#define BLINK "\x1b[5m"
#define REV "\x1b[7m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"

namespace {

const char* tr(Lang lang, const char* en, const char* es) {
  return lang == Lang::ES ? es : en;
}

// Dígitos grandes: 5 filas x 3 columnas
const char* const DIGITS[10][5] = {
    {"███", "█ █", "█ █", "█ █", "███"},  // 0
    {" █ ", " █ ", " █ ", " █ ", " █ "},  // 1
    {"███", "  █", "███", "█  ", "███"},  // 2
    {"███", "  █", "███", "  █", "███"},  // 3
    {"█ █", "█ █", "███", "  █", "  █"},  // 4
    {"███", "█  ", "███", "  █", "███"},  // 5
    {"███", "█  ", "███", "█ █", "███"},  // 6
    {"███", "  █", "  █", "  █", "  █"},  // 7
    {"███", "█ █", "███", "█ █", "███"},  // 8
    {"███", "█ █", "███", "  █", "███"},  // 9
};
const char* const COLON[5] = {"   ", " █ ", "   ", " █ ", "   "};

std::string rep(const char* s, int n) {
  std::string r;
  r.reserve(static_cast<size_t>(n) * 4);
  for (int i = 0; i < n; ++i) r += s;
  return r;
}

// Ancho en celdas de terminal (los caracteres UTF-8 de 3 bytes cuentan 1)
int wcells(const std::string& s) {
  int n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) ++n;
  return n;
}

std::string status_text(State st, Lang lang) {
  switch (st) {
    case State::IDLE:
      return tr(lang, "READY  ·  press SPACE to start",
                      "LISTO  ·  pulsa ESPACIO para empezar");
    case State::RUNNING:
      return tr(lang, "RUNNING", "EN MARCHA");
    case State::PAUSED:
      return tr(lang, "PAUSED", "PAUSADO");
    case State::FINISHED:
      return tr(lang, "TIME'S UP", "TIEMPO CUMPLIDO");
    case State::SW_IDLE:
      return tr(lang, "STOPWATCH READY  ·  press SPACE",
                      "CRONÓMETRO LISTO  ·  pulsa ESPACIO");
    case State::SW_RUN:
      return tr(lang, "STOPWATCH RUNNING", "CRONÓMETRO EN MARCHA");
    case State::SW_PAUSED:
      return tr(lang, "STOPWATCH PAUSED", "CRONÓMETRO PAUSADO");
    case State::SETTING:
      return tr(lang, "SET DURATION", "DEFINIR DURACIÓN");
  }
  return "";
}

const char* state_color(State st) {
  switch (st) {
    case State::RUNNING:
    case State::SW_RUN:    return GREEN;
    case State::PAUSED:
    case State::SW_PAUSED: return YELLOW;
    case State::FINISHED:  return RED;
    case State::SETTING:   return CYAN;
    default:               return WHITE;
  }
}

}  // namespace

std::string fmt_mmss(double seconds) {
  if (seconds < 0) seconds = 0;
  long long s = static_cast<long long>(seconds + 1e-9);
  char b[32];
  if (s >= 3600)
    std::snprintf(b, sizeof b, "%lld:%02lld:%02lld", s / 3600, (s % 3600) / 60,
                  s % 60);
  else
    std::snprintf(b, sizeof b, "%02lld:%02lld", s / 60, s % 60);
  return b;
}

std::string fmt_precise(double seconds) {
  if (seconds < 0) seconds = 0;
  long long cs = static_cast<long long>(seconds * 100 + 0.5);
  long long h = cs / 360000;
  long long m = (cs / 6000) % 60;
  long long s = (cs / 100) % 60;
  long long c = cs % 100;
  char b[32];
  if (h > 0)
    std::snprintf(b, sizeof b, "%lld:%02lld:%02lld.%02lld", h, m, s, c);
  else
    std::snprintf(b, sizeof b, "%lld:%02lld.%02lld", m, s, c);
  return b;
}

std::vector<std::string> big_lines(const std::string& text) {
  std::vector<std::string> out(5);
  for (size_t i = 0; i < text.size(); ++i) {
    const char* const* seg = nullptr;
    char c = text[i];
    if (c >= '0' && c <= '9')
      seg = DIGITS[c - '0'];
    else if (c == ':')
      seg = COLON;
    else
      continue;
    for (int r = 0; r < 5; ++r) {
      out[r] += seg[r];
      if (i + 1 < text.size()) out[r] += ' ';
    }
  }
  return out;
}

void render(const RenderInfo& I, int W, int H) {
  auto t = [&](const char* en, const char* es) { return tr(I.lang, en, es); };

  if (W < 42 || H < 16) {
    std::string msg =
        t("Terminal too small  ·  minimum 42x16",
          "Terminal demasiado pequeña  ·  mínimo 42x16");
    std::string out;
    out += "\x1b[2J\x1b[H";
    int x = std::max(0, (W - wcells(msg)) / 2);
    int y = H > 0 ? H / 2 : 0;
    out += "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
    out += YELLOW + msg + RST;
    std::cout << out << std::flush;
    return;
  }

  std::string out;
  out += "\x1b[2J\x1b[H";

  auto put = [&](int y, int x, const std::string& s) {
    if (y < 0 || y >= H || x < 0 || x >= W) return;
    out += "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H" +
           s;
  };
  auto center_vis = [&](int y, const std::string& vis,
                        const std::string& colored) {
    if (y < 0 || y >= H) return;
    int x = 1 + std::max(0, (W - 2 - wcells(vis)) / 2);
    put(y, x, colored);
  };
  auto center = [&](int y, const std::string& text, const char* color) {
    center_vis(y, text, color + text + RST);
  };

  // Marco
  put(0, 0, CYAN "╭" + rep("─", W - 2) + "╮" RST);
  put(H - 1, 0, CYAN "╰" + rep("─", W - 2) + "╯" RST);
  for (int r = 1; r < H - 1; ++r) {
    put(r, 0, CYAN "│" RST);
    put(r, W - 1, CYAN "│" RST);
  }

  // Título
  center(1, "T U I   T I M E R", CYAN BOLD);

  // Modo · estado
  std::string mode_name =
      (I.mode == Mode::COUNTDOWN) ? t("COUNTDOWN", "CUENTA ATRÁS")
                                  : t("STOPWATCH", "CRONÓMETRO");
  std::string st_line = mode_name + "   ·   " + status_text(I.state, I.lang);
  center(2, st_line, state_color(I.state));

  if (I.state == State::SETTING) {
    // Pantalla de definición de duración
    center(4, t("Enter the duration:", "Introduce la duración:"), CYAN BOLD);

    std::string shown = I.input;
    if (wcells(shown) > W - 12) {
      while (wcells(shown) > W - 12) shown = shown.substr(1);
      shown = "…" + shown;
    }
    if (shown.empty()) shown = "  ";
    std::string vis = " " + shown + " ";
    center_vis(5, vis, REV " " + shown + " " RST);

    std::string fmt = t("Formats:  5:30  ·  90 (seconds)  ·  2m  ·  1h",
                        "Formatos:  5:30  ·  90 (segundos)  ·  2m  ·  1h");
    center_vis(7, fmt, DIM + fmt + RST);
    if (I.input_error) {
      std::string err =
          t("Invalid duration. Try again.", "Duración no válida. Inténtalo de nuevo.");
      center_vis(8, err, RED + err + RST);
    }
  } else {
    // Dígitos grandes
    std::string big_text = (I.mode == Mode::COUNTDOWN)
                               ? fmt_mmss(std::max(0.0, I.remaining))
                               : fmt_mmss(I.elapsed);
    auto lines = big_lines(big_text);
    const char* big_col;
    switch (I.state) {
      case State::FINISHED: big_col = RED BOLD BLINK; break;
      case State::PAUSED:
      case State::SW_PAUSED: big_col = YELLOW BOLD; break;
      case State::RUNNING:
      case State::SW_RUN: big_col = GREEN BOLD; break;
      default: big_col = WHITE BOLD; break;
    }
    int r = 4;
    for (auto& l : lines) center(r++, l, big_col);
  }

  if (I.mode == Mode::COUNTDOWN && I.state != State::SETTING) {
    // Barra de progreso
    double frac = (I.total > 0)
                      ? std::clamp(1.0 - I.remaining / I.total, 0.0, 1.0)
                      : 0.0;
    int bw = std::max(8, std::min(44, W - 24));
    int filled = static_cast<int>(frac * bw);
    std::string barvis = "[" + rep("█", filled) + rep("░", bw - filled) + "]";
    char pct[16];
    std::snprintf(pct, sizeof pct, "  %3d%%", (int)(frac * 100 + 0.5));
    center_vis(10, barvis + pct,
               CYAN + barvis + RST " " + WHITE + pct + RST);

    // Línea de información
    if (I.state == State::FINISHED) {
      std::string done = t("***  TIME'S UP!  ***", "***  ¡TIEMPO CUMPLIDO!  ***");
      center_vis(11, done, RED BOLD BLINK + done + RST);
    } else {
      std::string vis = t("Total ", "Total ") + fmt_mmss(I.total) +
                        t("    ·    Remaining ", "    ·    Quedan ") +
                        fmt_mmss(std::max(0.0, I.remaining));
      center_vis(11, vis, DIM + vis + RST);
    }
  } else {
    std::string vis = t("Exact time: ", "Tiempo exacto: ") +
                      fmt_precise(I.elapsed);
    center_vis(10, vis, DIM + vis + RST);
    std::string hint = t("Press SPACE to start or stop",
                         "Pulsa ESPACIO para empezar o parar");
    center_vis(11, hint, DIM + hint + RST);
  }

  // Ayuda (abajo)
  int hy = H - 3;
  if (I.state == State::SETTING) {
    center(hy, "[0-9 :] escribir     [ENTER] aceptar     [ESC] cancelar",
           DIM);
    center(hy + 1, "Puedes escribir 5:30, 90, 2m o 1h", DIM);
  } else if (I.mode == Mode::COUNTDOWN) {
    center(hy, "[ESPACIO] empezar / pausar     [N] nueva duración     [S] reiniciar",
           DIM);
    center(hy + 1, "[M] cronómetro     [Q] salir", DIM);
  } else {
    center(hy, "[ESPACIO] empezar / parar     [S] reiniciar     [M] cuenta atrás",
           DIM);
    center(hy + 1, "[Q] salir", DIM);
  }

  std::cout << out << std::flush;
}

}  // namespace tui
