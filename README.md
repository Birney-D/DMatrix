# DMatrix

A C++ terminal-based digital rain simulator inspired by *The Matrix*. Uses ANSI escape sequences to render animated streams of characters directly in the terminal.

## Features

- **Animated digital rain** with configurable downward or upward movement.
- **Terminal resizing support** — updates the simulation when the terminal dimensions change.
- **Color controls** — switch between red, green, blue, and yellow.
- **Text intensity controls** — bold and dim display modes.
- **Adjustable simulation speed** using number keys 1–9.
- **Buffered terminal output** — builds terminal escape sequences in a buffer and writes the output once per simulation iteration.
- **Graceful shutdown** using `q`.

## Build

Requires a C++ compiler and a Unix-like terminal environment.

```bash
make
```

Alternatively, compile the source directly with your C++ compiler:

```bash
g++ -std=c++17 -Wall -Wextra -O2 dmatrix.cpp -o dmatrix
```

## Usage

```bash
./dmatrix [-u | -d] [-B | -b]
```

### Command-line options

 | Option | Description                            |
 --------------------------------------------------|
 | `-u`   | Make character streams travel upward   |
 | `-d`   | Make character streams travel downward |
 | `-B`   | Use bold characters                    |
 | `-b`   | Use dim characters                     |

** Character streams travel downward by default when no direction option is provided. **

### Runtime controls
_____________________________________
|   Key   |     Action              |
|-----------------------------------|
| `r`     | Red characters          |
| `g`     | Green characters        |
| `b`     | Blue characters         |
| `y`     | Yellow characters       |
| `1`–`9` | Adjust simulation speed |
| `q`     | Gracefully quit         |
|-----------------------------------|

## Implementation

The simulation uses ANSI escape sequences to control terminal cursor positioning and character styling. 
Output is accumulated in a buffer and written using `write()` once per simulation iteration.

Terminal resizing is handled dynamically so the simulation can adapt to changes in the terminal dimensions while running.

## License

MIT License.
