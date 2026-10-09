// initialize_board.go generates a playable Sudoku puzzle in Go and uses the
// C game_state/game_loop API declared by backend.h.
//
// Keep this file in the same directory as backend.h and game_loop.c.
// Build the package (rather than this file alone) so cgo compiles game_loop.c:
//
//     go mod init sudoku   # only once, if the directory has no go.mod
//     go build -o sudoku .
//     ./sudoku -clues 30
//
// The executable is a newline-delimited JSON service for a Python UI.
// It writes a state snapshot at startup and after every command. Commands:
//     {"action":"set","row":0,"col":2,"num":7}
//     {"action":"new","clues":32}
//     {"action":"quit"}
// Coordinates are zero-based; num=0 clears a player-entered cell.

package main

/*
#cgo CFLAGS: -std=gnu2x
#include <stdint.h>
#include "backend.h"

// game_loop returns game_state by value, which cgo does not expose directly
// to Go. This C wrapper keeps the struct on the C side and updates it in place.
game_state game_loop(game_state gs);
static inline void sudoku_go_reset(game_state *gs) {
	*gs = (game_state){0};
}
static inline void sudoku_go_set_num(game_state *gs, int row, int col, uint8_t n) {
	gs->board[row][col].num = n;
  gs->board[row][col].state = NONE;
}
static inline void sudoku_go_validate(game_state *gs) {
	*gs = game_loop(*gs);
  // The supplied game_loop.c may not calculate has_won itself.
  gs->has_won = true;
  for (int i = 0; i < 9; ++i) {
    if (!gs->row_is_right[i] || !gs->column_is_right[i] || !gs->box_is_right[i]) {
      gs->has_won = false;
      break;
    }
  }
}
static inline int sudoku_go_num(const game_state *gs, int row, int col) {
  return (int)gs->board[row][col].num;
}
static inline int sudoku_go_cell_state(const game_state *gs, int row, int col) {
  return (int)gs->board[row][col].state;
}
static inline int sudoku_go_row_right(const game_state *gs, int i) {
  return gs->row_is_right[i] ? 1 : 0;
}
static inline int sudoku_go_column_right(const game_state *gs, int i) {
  return gs->column_is_right[i] ? 1 : 0;
}
static inline int sudoku_go_box_right(const game_state *gs, int i) {
  return gs->box_is_right[i] ? 1 : 0;
}
static inline int sudoku_go_has_won(const game_state *gs) {
  return gs->has_won ? 1 : 0;
}
*/
import "C"

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"math/bits"
	"math/rand"
	"os"
	"time"
)

const (
	size         = 9
	cellCount    = size * size
	allDigits    = uint16(0x3FE) // bits 1 through 9
	defaultClues = 30
)

type grid [cellCount]uint8

func candidates(g *grid, index int) uint16 {
	row, col := index/size, index%size
	var used uint16
	for i := 0; i < size; i++ {
		if n := g[row*size+i]; n != 0 {
			used |= uint16(1) << n
		}
		if n := g[i*size+col]; n != 0 {
			used |= uint16(1) << n
		}
	}
	boxRow, boxCol := (row/3)*3, (col/3)*3
	for r := boxRow; r < boxRow+3; r++ {
		for c := boxCol; c < boxCol+3; c++ {
			if n := g[r*size+c]; n != 0 {
				used |= uint16(1) << n
			}
		}
	}
	return allDigits &^ used
}

func fillRandom(g *grid, rng *rand.Rand) bool {
	bestCount := size + 1
	bestCells := make([]int, 0, cellCount)
	for i, n := range g {
		if n != 0 {
			continue
		}
		mask := candidates(g, i)
		count := bits.OnesCount16(mask)
		if count == 0 {
			return false
		}
		if count < bestCount {
			bestCount = count
			bestCells = bestCells[:0]
			bestCells = append(bestCells, i)
		} else if count == bestCount {
			bestCells = append(bestCells, i)
		}
	}
	if len(bestCells) == 0 {
		return true
	}

	index := bestCells[rng.Intn(len(bestCells))]
	mask := candidates(g, index)
	values := make([]uint8, 0, size)
	for n := uint8(1); n <= 9; n++ {
		if mask&(uint16(1)<<n) != 0 {
			values = append(values, n)
		}
	}
	rng.Shuffle(len(values), func(i, j int) { values[i], values[j] = values[j], values[i] })
	for _, n := range values {
		g[index] = n
		if fillRandom(g, rng) {
			return true
		}
		g[index] = 0
	}
	return false
}

// countSolutions counts at most limit solutions, enough to test uniqueness.
// The grid is restored before returning.
func countSolutions(g *grid, limit int) int {
	if limit <= 0 {
		return 0
	}
	bestIndex, bestMask, bestCount := -1, uint16(0), size+1
	for i, n := range g {
		if n != 0 {
			continue
		}
		mask := candidates(g, i)
		count := bits.OnesCount16(mask)
		if count == 0 {
			return 0
		}
		if count < bestCount {
			bestIndex, bestMask, bestCount = i, mask, count
			if count == 1 {
				break
			}
		}
	}
	if bestIndex == -1 {
		return 1
	}
	count := 0
	for n := uint8(1); n <= 9; n++ {
		if bestMask&(uint16(1)<<n) == 0 {
			continue
		}
		g[bestIndex] = n
		count += countSolutions(g, limit-count)
		g[bestIndex] = 0
		if count >= limit {
			return limit
		}
	}
	return count
}

func generatePuzzle(targetClues int, rng *rand.Rand) (grid, error) {
	var solution grid
	if !fillRandom(&solution, rng) {
		return grid{}, fmt.Errorf("could not generate a complete Sudoku grid")
	}
	puzzle := solution
	order := rng.Perm(cellCount)
	clues := cellCount
	for _, index := range order {
		if clues <= targetClues {
			break
		}
		old := puzzle[index]
		puzzle[index] = 0
		if countSolutions(&puzzle, 2) == 1 {
			clues--
		} else {
			puzzle[index] = old
		}
	}
	return puzzle, nil
}

type command struct {
	Action string `json:"action"`
	Row    *int   `json:"row,omitempty"`
	Col    *int   `json:"col,omitempty"`
	Num    *int   `json:"num,omitempty"`
	Clues  *int   `json:"clues,omitempty"`
}

type response struct {
	Board         [9][9]uint8  `json:"board"`
	CellStates    [9][9]string `json:"cell_states"`
	Given         [9][9]bool   `json:"given"`
	RowsSolved    [9]bool      `json:"row_is_right"`
	ColumnsSolved [9]bool      `json:"column_is_right"`
	BoxesSolved   [9]bool      `json:"box_is_right"`
	HasWon        bool         `json:"has_won"`
	Error         string       `json:"error,omitempty"`
}

var (
	state             C.game_state
	given             [cellCount]bool
	rng               *rand.Rand
	currentClueTarget = defaultClues
)

func installPuzzle(puzzle grid) {
	C.sudoku_go_reset(&state)
	for i, n := range puzzle {
		row, col := i/size, i%size
		C.sudoku_go_set_num(&state, C.int(row), C.int(col), C.uint8_t(n))
		given[i] = n != 0
	}
	C.sudoku_go_validate(&state)
}

func currentResponse(errText string) response {
	var out response
	out.Error = errText
	for i := 0; i < cellCount; i++ {
		row, col := i/size, i%size
		out.Board[row][col] = uint8(C.sudoku_go_num(&state, C.int(row), C.int(col)))
		out.Given[row][col] = given[i]
		switch C.sudoku_go_cell_state(&state, C.int(row), C.int(col)) {
		case 0:
			out.CellStates[row][col] = "NONE"
		case 1:
			out.CellStates[row][col] = "INCORRECT"
		default:
			out.CellStates[row][col] = "UNKNOWN"
		}
	}
	for i := 0; i < size; i++ {
		ci := C.int(i)
		out.RowsSolved[i] = C.sudoku_go_row_right(&state, ci) != 0
		out.ColumnsSolved[i] = C.sudoku_go_column_right(&state, ci) != 0
		out.BoxesSolved[i] = C.sudoku_go_box_right(&state, ci) != 0
	}
	out.HasWon = C.sudoku_go_has_won(&state) != 0
	return out
}

func emit(errText string) {
	if err := json.NewEncoder(os.Stdout).Encode(currentResponse(errText)); err != nil {
		fmt.Fprintln(os.Stderr, "encode response:", err)
	}
}

func handleCommand(cmd command) (quit bool) {
	switch cmd.Action {
	case "quit":
		return true
	case "new":
		target := currentClueTarget
		if cmd.Clues != nil {
			target = *cmd.Clues
		}
		if target < 17 || target > 80 {
			emit("clues must be between 17 and 80")
			return false
		}
		puzzle, err := generatePuzzle(target, rng)
		if err != nil {
			emit(err.Error())
			return false
		}
		currentClueTarget = target
		installPuzzle(puzzle)
		emit("")
	case "set":
		if cmd.Row == nil || cmd.Col == nil || cmd.Num == nil {
			emit("set requires row, col, and num")
			return false
		}
		row, col, num := *cmd.Row, *cmd.Col, *cmd.Num
		if row < 0 || row >= size || col < 0 || col >= size {
			emit("row and col must be between 0 and 8")
			return false
		}
		if num < 0 || num > 9 {
			emit("num must be between 0 and 9; use 0 to clear a cell")
			return false
		}
		index := row*size + col
		if given[index] {
			emit("cannot modify a given clue")
			return false
		}
		C.sudoku_go_set_num(&state, C.int(row), C.int(col), C.uint8_t(num))
		C.sudoku_go_validate(&state)
		emit("")
	default:
		emit("unknown action; expected set, new, or quit")
	}
	return false
}

func main() {
	cluesFlag := flag.Int("clues", defaultClues, "target clue count for the first/new puzzle (17-80)")
	seedFlag := flag.Int64("seed", 0, "random seed (0 uses the current time)")
	flag.Parse()
	if *cluesFlag < 17 || *cluesFlag > 80 {
		fmt.Fprintln(os.Stderr, "-clues must be between 17 and 80")
		os.Exit(2)
	}
	seed := *seedFlag
	if seed == 0 {
		seed = time.Now().UnixNano()
	}
	rng = rand.New(rand.NewSource(seed))
	currentClueTarget = *cluesFlag

	puzzle, err := generatePuzzle(currentClueTarget, rng)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	installPuzzle(puzzle)
	emit("")

	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 4096), 1024*1024)
	for scanner.Scan() {
		var cmd command
		if err := json.Unmarshal(scanner.Bytes(), &cmd); err != nil {
			emit("invalid JSON command: " + err.Error())
			continue
		}
		if handleCommand(cmd) {
			return
		}
	}
	if err := scanner.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "read command:", err)
	}
}
