package main

import "C"

import (
	"math/bits"
	"math/rand"
	"time"
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

func makeSolvedBoard(board *grid) {
	rng := rand.New(rand.NewSource(time.Now().UnixNano()))
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

func main() {}
