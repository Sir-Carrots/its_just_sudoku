#include "backend.h"
#include <stdio.h>

int main(void) {
  game_state unsolved_puzzle = {0};
  if (read_from_csv(&unsolved_puzzle, "~/Projects/its_just_sudoku/test_boards/valid_puzzle.csv") != 0) {
    perror("failed to read file");
    return -1;
  }
  game_state output1 = game_loop(unsolved_puzzle);
  fwrite(&output1, sizeof(output1), 1, stdout); 

  game_state solved_puzzle = {0};
  if (read_from_csv(&solved_puzzle, "~/Projects/its_just_sudoku/test_boards/solved_board.csv") != 0) {
    perror("failed to read file");
    return -1;
  }
  game_state output2 = game_loop(solved_puzzle);
  fwrite(&output2, sizeof(output2), 1, stdout); 

  game_state conflict_puzzle = {0};
  if (read_from_csv(&conflict_puzzle, "~/Projects/its_just_sudoku/test_boards/conflict_board.csv") != 0) {
    perror("failed to read file");
    return -1;
  }
  game_state output3 = game_loop(conflict_puzzle);
  fwrite(&output3, sizeof(output3), 1, stdout);
  
  return 0;
}
