#include "backend.h"
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>

typedef struct pack_game_state {
  game_state one;
  game_state two;
  game_state three;
} pack_game_state;

int main(void) {
  game_state test[3] = {0};
  game_state unsolved_puzzle = {0};
  if (read_from_csv(&unsolved_puzzle, "/home/atharvm/Projects/its_just_sudoku/test_boards/valid_puzzle.csv") != 0) {
    perror("failed to read file");
    return -1;
  }
  game_state output1 = game_loop(unsolved_puzzle);
  test[0] = output1;

  game_state solved_puzzle = {0};
  if (read_from_csv(&solved_puzzle, "/home/atharvm/Projects/its_just_sudoku/test_boards/solved_board.csv") != 0) {
    perror("failed to read file");
    return -1;
  }
  game_state output2 = game_loop(solved_puzzle);
  test[1] = output2;

  game_state conflict_puzzle = {0};
  if (read_from_csv(&conflict_puzzle, "/home/atharvm/Projects/its_just_sudoku/test_boards/conflict_board.csv") != 0) {
    perror("failed to read file");
    return -1;
  }
  game_state output3 = game_loop(conflict_puzzle);
  test[2] = output3;
  FILE *out = fopen("./bin/output.txt", "wb");
  
  size_t buff_size = fwrite(&test, sizeof(game_state), 3, out);
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 9; j++) {
      for (int k = 0; k < 9; k++) {
        printf("%" PRIu8 ":%" PRIu8 " ", test[i].nums[j][k], test[i].state[j][k]);
      }
      printf("%b\n", test[i].row_is_right[j]);
    }
    for (int j = 0; j < 9; j++) {
      printf("%b   ", test[i].column_is_right[j]);
    }
    printf("\n");
    for (int j = 0; j < 9; j++) {
      printf("%b ", test[i].box_is_right[j]);
    }
    printf("%b\n\n\n", test[i].has_won);
  }
  
  return (int)buff_size;
}
