#pragma once

#include <string>
#include <vector>

namespace tui {

enum class Mode { COUNTDOWN, STOPWATCH };

enum class State {
  IDLE,       // cuenta atrás lista para empezar
  RUNNING,    // cuenta atrás en marcha
  PAUSED,     // cuenta atrás pausada
  FINISHED,   // cuenta atrás terminada
  SW_IDLE,    // cronómetro a cero
  SW_RUN,     // cronómetro en marcha
  SW_PAUSED,  // cronómetro pausado
  SETTING,    // introduciendo duración
};

struct RenderInfo {
  Mode mode = Mode::COUNTDOWN;
  State state = State::IDLE;
  double remaining = 0.0;  // segundos restantes (cuenta atrás)
  double total = 0.0;      // duración total en segundos (cuenta atrás)
  double elapsed = 0.0;    // tiempo transcurrido (cronómetro)
  std::string input;       // texto tecleado en el modo definición
  bool input_error = false;
};

// "05:30" o "1:02:30" si hay horas
std::string fmt_mmss(double seconds);
// "4:59.37" con centésimas (cronómetro)
std::string fmt_precise(double seconds);
// Genera 5 líneas con el texto en dígitos grandes
std::vector<std::string> big_lines(const std::string& text);

void render(const RenderInfo& info, int width, int height);

}  // namespace tui
