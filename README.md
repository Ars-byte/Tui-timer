**English** · [Español](README.es.md)

# TUI Timer

Stopwatch and countdown with a comfortable TUI, in C++17 and **no
dependencies** (pure ANSI + termios, no ncurses needed).

## Build and run

```sh
cmake -B build
cmake --build build
./build/tui-timer          # with the default duration (5:00)
./build/tui-timer 25:00    # or start with a different duration
```

## Controls

| Key      | Action                                     |
| -------- | ------------------------------------------ |
| `SPACE`  | Start / pause (or resume)                  |
| `N`      | New duration (countdown)                   |
| `S`      | Stop / reset                               |
| `M`      | Switch between countdown and stopwatch     |
| `L`      | Switch language (English / Spanish)        |
| `Q`      | Quit                                       |

### Entering a duration (screen `N`)

- `5:30`  → 5 minutes 30 seconds
- `90`    → 90 seconds
- `2m`    → 2 minutes
- `1h`    → 1 hour
- `ESC`   → cancel

## Features

- Big digits, progress bar and colors depending on the state
  (green running, yellow paused, blinking red when finished).
- Beep when it finishes and every 2 seconds while finished.
- Countdown and stopwatch (with hundredths).
- Adapts to the terminal size and can be paused/resumed.
- The terminal is restored to its original state on exit.

<img width="629" height="465" alt="image" src="https://github.com/user-attachments/assets/f99c8d8e-dca4-4b1b-b879-e444f96bb2dc" />


## Structure

```
tui-timer/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp   # main loop, input and states
    ├── ui.hpp     # UI interface
    └── ui.cpp     # ANSI rendering (frame, digits, bars)
```
