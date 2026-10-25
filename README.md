# It's just sudoku


## Intro
It is literally just sudoku. So, the things work talking about are:

- It uses a c board validator, which completes the board validation in 400~800 ns (on my machine)
- this can be improved further: 
  - if you let go of all the extras that the current validator provide, then you can pack two cells into one uint8_t by using the upper and lower four bits for each cell
  - This will allow you to pack the board down to around 45 bytes, which means you can vectorize the whole thing!
  - Again, this is pretty vague, because I haven't looked into it that much, but you could get it down to ~50 ns (try looking at the tdoku repo https://github.com/t-dillon/tdoku. The solver is very cool)
- The go initializer cannot make all possible solvable sudoku boards, because it uses a presolved sudoku pattern to make a puzzle, instead of starting from scratch and using a recursive backtracking solver. Oops
- The game has vim-motions. Use them (I promise you it's way better)


## Usage

### Requirements

The commands below target Linux. You need:

- GCC with C23 support, including fixed-underlying-type enums (GCC 15 works).
- Go 1.22 or newer, with CGo enabled.
- Python 3 with Tkinter and a graphical desktop session.

No pip packages, virtual environment, or Python build step are needed. On Arch/CachyOS, the dependencies can be installed with:

```sh
sudo pacman -Syu --needed gcc go python tk
```

Use the system Python so it picks up the Tk installation from pacman.

### Build and run

Run these commands from the root of this repository. `bin/` is a local directory, not the system `/bin`; if it already exists, `mkdir -p` leaves it alone.

```sh
mkdir -p bin
```

Build the C validator and CSV functions into a shared library:

```sh
gcc -std=c23 -O2 -Wall -Wextra -fPIC -shared game_loop.c import_export.c -o bin/libsudoku.so
```

Build the Go initializer into a separate shared library. Pass the Go filename explicitly so Go does not try to build the unrelated C programs in this directory:

```sh
CGO_ENABLED=1 go build -buildmode=c-shared -o bin/libgenerator.so intialize_board.go
```

Launch the frontend:

```sh
python3 frontend.py
```

On Arch/CachyOS, use `/usr/bin/python frontend.py` if your shell's Python points to a virtual environment or another installation. You can check Tkinter with `/usr/bin/python -m tkinter`; it should open a small demo window.

After the first build, just run the Python command to play. Rebuild the relevant shared library whenever you change its C or Go source. Python-only changes do not require rebuilding either library.

### Playing

A puzzle is generated on startup. **New puzzle** generates another one; its bold starting clues are locked. Click a cell to select it, then use:

| Input                     | Action                                                  |
| ------------------------- | ------------------------------------------------------- |
| `1`–`9`                   | Enter a number in an editable cell                      |
| Backspace, Delete, or `0` | Clear an editable cell                                  |
| Arrow keys                | Move the selection                                      |
| `h`, `j`, `k`, `l`        | Move left, down, up, right when **Vim keys** is enabled |
| `x`                       | Clear an editable cell when **Vim keys** is enabled     |

Movement wraps at the board edges. Enable vim keys using the checkbox in the toolbar. There is no separate insert mode, and arrow keys and mouse input still work with Vim keys enabled.

- Terracotta cells indicate conflicts.
- Sage-green cells belong to a completed row, column, or 3×3 box; conflicts take priority over completion highlighting.
- Checkmarks indicate completed rows and columns. Completed boxes are listed below the board, numbered 1–9 from left to right, top to bottom.
- **Solved!** appears when the C backend reports that the whole board is complete.

### Save and load

Use **Save CSV** to save the current board and **Load CSV** to open one. The format is nine rows of nine comma-separated integers from `0` to `9`, with `0` meaning an empty cell. Supply correctly formatted files; the C CSV reader is not intended as a hardened parser for untrusted input.

CSV stores only the numbers, not which cells were original clues. **Every cell in a loaded board is editable**, even if the file was saved from a generated puzzle. Loading or generating a board replaces the current game without an unsaved-change prompt, so save first if you want to keep it.

### Benchmarking the C validator

To build and run the standalone validation microbenchmark:

```sh
gcc -std=c23 -O2 -Wall -Wextra bench.c game_loop.c -o bin/bench_game_loop
./bin/bench_game_loop 3000000 valid
./bin/bench_game_loop 3000000 conflict
```

The first argument is the iteration count; the second selects the nonconflicting or conflicting board. The reported `ns_per_call` measures repeated validation of one hot-in-cache board, not CSV operations or frontend latency. Build without LTO and repeat runs to compare timings; results depend on your CPU and compiler.


## Releases
I will put them out wait pls.


## Why
Good question. My friends had picked this up as their one of their highchool projects and I thought it was fun so I made it.
