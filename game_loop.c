#include "game_loop.h"
#include <stdint.h>

INLINE void row_check(game_state *gs, uint_fast8_t row_num) {
  uint16_t seen_posns = 0;
  uint16_t duplicate_posns = 0;

  for (uint_fast8_t i = 0; i < 9; i++) {
    uint_fast8_t num = gs->board[row_num][i].num;
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (seen_posns & bit_posn) {
      duplicate_posns |= bit_posn;
    }
    seen_posns |= bit_posn;
  }

  for (uint_fast8_t i = 0; i < 9; i++) {
    uint_fast8_t num = gs->board[row_num][i].num;
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (duplicate_posns & bit_posn){
      gs->board[row_num][i].state = INCORRECT;
    }
    if (num == 0) {
      gs->board[row_num][i].state = NONE;
    }
  }

  gs->row_is_right[row_num] = (seen_posns == 0b0000001111111110 
    && duplicate_posns == 0b0000000000000000); //this is a fully solved row's state

  return;
}

INLINE void column_check (game_state *gs, uint_fast8_t column_num) {
  uint16_t seen_posns = 0;
  uint16_t duplicate_posns = 0;

  for (uint_fast8_t i = 0; i < 9; i++) {
    uint_fast8_t num = gs->board[i][column_num].num;
    if (num == 0) {
      continue;
    }
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (seen_posns & bit_posn) {
      duplicate_posns |= bit_posn;
    }
    seen_posns |= bit_posn;
  }

  for (uint_fast8_t i = 0; i < 9; i++) {
    uint_fast8_t num = gs->board[i][column_num].num;
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (duplicate_posns & bit_posn) {
      gs->board[i][column_num].state = INCORRECT;
    }
    if (num == 0) {
      gs->board[i][column_num].state = NONE;
    }
  }

  gs->column_is_right[column_num] = (seen_posns == 0b0000001111111110 
    && duplicate_posns == 0b0000000000000000);

  return;
}

INLINE void box_check (game_state *gs, uint_fast8_t box_num) {
  uint16_t seen_posns = 0;
  uint16_t duplicate_posns = 0;

  for (uint_fast8_t i = 0; i < 9; i++) {
    uint_fast8_t row = (box_num / 3) * 3 + (i / 3);
    uint_fast8_t col = (box_num % 3) * 3 + (i % 3);
    uint_fast8_t num = gs->board[row][col].num;
    uint16_t bit_posn = (uint16_t)(1u << num);
    if(seen_posns & bit_posn) {
      duplicate_posns |= bit_posn;
    }
    seen_posns |= bit_posn;
  }

  for (uint_fast8_t i = 0; i < 9; i++) {
    uint_fast8_t row = (box_num / 3) * 3 + (i / 3);
    uint_fast8_t col = (box_num % 3) * 3 + (i % 3);
    uint_fast8_t num = gs->board[row][col].num;
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (duplicate_posns & bit_posn) {
      gs->board[row][col].state = INCORRECT;
    }
    if (num == 0) {
      gs->board[row][col].state = NONE;
    }
  }

  gs->box_is_right[box_num] = (seen_posns == 0b0000001111111110 
    && duplicate_posns == 0b0000000000000000);

  return;
}

game_state game_loop(game_state gs) {
  for (uint_fast8_t i = 0; i<81; i++) {
    gs.board[i]->state = NONE;
  }
  for (uint_fast8_t i = 0; i < 9; i++) {
    row_check(&gs, i);
  }
  for (uint_fast8_t i = 0; i < 9; i++) {
    column_check(&gs, i);
  }
  for (uint_fast8_t i = 0; i < 9; i++) {
    box_check(&gs, i);
  }
  
  return gs;
}
