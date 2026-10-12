package main

/*
#cgo CFLAGS: -std=c23
#include <stdint.h>
*/
import "C"

import (
	"math/bits"
	"math/rand"
	"time"
	"unsafe"
)

const (
	size          int    = 9
	grid_size     int    = size * size
	all_posns     uint16 = 0b0000001111111110
	default_clues        = 30
)

type grid [size][size]uint8

func initGrid(board *grid) {
	for row := range size {
		for col := range size {
			board[row][col] = uint8((3*row+row/3+col)%9 + 1)
		}
	}
}

func shuffleInBand(board *grid, bandNum int, rng *rand.Rand) {
	startingRow := bandNum * 3

	rng.Shuffle(3, func(a, b int) {
		board[startingRow+a], board[startingRow+b] = board[startingRow+b], board[startingRow+a]
	})
}

func shuffleInStack(board *grid, stackNum int, rng *rand.Rand) {
	startingCol := stackNum * 3

	rng.Shuffle(3, func(a, b int) {
		for row := range size {
			board[row][startingCol+a], board[row][startingCol+b] =
				board[row][startingCol+b], board[row][startingCol+a]
		}
	})
}

func shuffleBands(board *grid, rng *rand.Rand) {
	rng.Shuffle(3, func(a, b int) {
		for offset := range 3 {
			rowA := 3*a + offset
			rowB := 3*b + offset
			board[rowA], board[rowB] = board[rowB], board[rowA]
		}
	})
}

func shuffleStacks(board *grid, rng *rand.Rand) {
	rng.Shuffle(3, func(a, b int) {
		for row := range size {
			for offset := range 3 {
				colA := 3*a + offset
				colB := 3*b + offset
				board[row][colA], board[row][colB] =
					board[row][colB], board[row][colA]
			}
		}
	})
}

func makeSolvedBoard(board *grid, rng *rand.Rand) {
	initGrid(board)
	for bandNum := range 3 {
		shuffleInBand(board, bandNum, rng)
	}
	shuffleBands(board, rng)
	for stackNum := range 3 {
		shuffleInStack(board, stackNum, rng)
	}
	shuffleStacks(board, rng)
}

func searchSolutions(
	board *grid,
	limit int,
	occupiedInRow, occupiedInCol, occupiedInBox *[size]uint16) int {
	bestRow, bestCol := -1, -1
	var bestOptions uint16
	bestCount := size + 1

	for row := range size {
		for col := range size {
			if board[row][col] != 0 {
				continue
			}
			box := (row/3)*3 + col/3
			options := all_posns &
				^(occupiedInRow[row] | occupiedInCol[col] | occupiedInBox[box])
			count := bits.OnesCount16(options)
			if count == 0 {
				return 0
			}
			if count < bestCount {
				bestRow, bestCol = row, col
				bestOptions = options
				bestCount = count
			}
		}
	}

	if bestRow == -1 {
		return 1
	}

	box := (bestRow/3)*3 + bestCol/3
	total := 0

	for digit := uint8(1); digit <= 9; digit++ {
		bit := uint16(1) << digit
		if bestOptions&bit == 0 {
			continue
		}
		board[bestRow][bestCol] = digit
		occupiedInRow[bestRow] |= bit
		occupiedInCol[bestCol] |= bit
		occupiedInBox[box] |= bit

		total += searchSolutions(
			board, limit-total,
			occupiedInRow, occupiedInCol, occupiedInBox,
		)

		board[bestRow][bestCol] = 0
		occupiedInRow[bestRow] &^= bit
		occupiedInCol[bestCol] &^= bit
		occupiedInBox[box] &^= bit

		if total >= limit {
			return limit
		}
	}

	return total
}

func countSolutions(board *grid, limit int) int {
	var occupiedInRow, occupiedInCol, occupiedInBox [size]uint16
	for row := range size {
		for col := range size {
			num := board[row][col]
			bitPosn := uint16(1) << num
			box := (row/3)*3 + col/3
			if occupiedInRow[row]&bitPosn != 0 || occupiedInCol[col]&bitPosn != 0 || occupiedInBox[box]&bitPosn != 0 {
				return 0
			}
			occupiedInRow[row] |= bitPosn
			occupiedInCol[col] |= bitPosn
			occupiedInBox[box] |= bitPosn
		}
	}
	return searchSolutions(board, limit, &occupiedInRow, &occupiedInCol, &occupiedInBox)
}

func makePuzzle(board *grid, rng *rand.Rand) int {
	positions := make([][2]int, grid_size)
	for i := range grid_size {
		positions[i] = [2]int{i / size, i % size}
	}

	rng.Shuffle(len(positions), func(i, j int) {
		positions[i], positions[j] = positions[j], positions[i]
	})
	clues := grid_size
	for _, pos := range positions {
		if clues <= default_clues {
			break
		}

		row, col := pos[0], pos[1]
		original := board[row][col]

		board[row][col] = 0

		candidate := *board
		solutions := countSolutions(&candidate, 2)

		if solutions == 1 {
			clues--
		} else {
			board[row][col] = original
		}
	}
	return clues
}

// export generate_puzzle
func generate_puzzle(boardPosn *C.uint8_t) C.int {
	rng := rand.New(rand.NewSource(time.Now().UnixNano()))
	board := (*grid)(unsafe.Pointer(boardPosn))
	makeSolvedBoard(board, rng)
	clues := makePuzzle(board, rng)
	return C.int(clues)
}

func main() {}
