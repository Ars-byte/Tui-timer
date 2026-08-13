# TUI Timer

Cronómetro y cuenta atrás con interfaz TUI cómoda, en C++17 y **sin
dependencias** (ANSI puro + termios, no necesita ncurses).

## Compilar y ejecutar

```sh
cmake -B build
cmake --build build
./build/tui-timer          # con la duración por defecto (5:00)
./build/tui-timer 25:00    # o arranca ya con otra duración
```

## Controles

| Tecla      | Acción                                        |
| ---------- | --------------------------------------------- |
| `ESPACIO`  | Empezar / pausar (o reanudar)                 |
| `N`        | Nueva duración (cuenta atrás)                 |
| `S`        | Detener / reiniciar                           |
| `M`        | Cambiar entre cuenta atrás y cronómetro       |
| `Q`        | Salir                                         |

### Escribir una duración (pantalla `N`)

- `5:30`  → 5 minutos 30 segundos
- `90`    → 90 segundos
- `2m`    → 2 minutos
- `1h`    → 1 hora
- `ESC`   → cancelar

## Características

- Dígitos grandes, barra de progreso y colores según el estado
  (verde en marcha, amarillo pausado, rojo parpadeante al terminar).
- Pitido al terminar y cada 2 segundos mientras esté finalizado.
- Cuenta atrás y cronómetro (con centésimas).
- Se adapta al tamaño de la terminal y se puede pausar/retomar.
- La terminal vuelve a su estado original al salir.

## Estructura

```
tui-timer/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp   # bucle principal, entrada y estados
    ├── ui.hpp     # interfaz de la UI
    └── ui.cpp     # renderizado ANSI (marco, dígitos, barras)
```
