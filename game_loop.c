#include "backend.h"
#include <string.h>

INLINE bool row_check(game_state *gs, uint8_t row_num) {
  uint16_t seen_posns = 0;
  uint16_t duplicate_posns = 0;

  for (uint8_t i = 0; i < 9; i++) {
    uint8_t num = gs->nums[row_num][i];
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (seen_posns & bit_posn) {
      duplicate_posns |= bit_posn;
    }
    seen_posns |= bit_posn;
  }

  for (uint8_t i = 0; i < 9; i++) {
    uint8_t num = gs->nums[row_num][i];
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (duplicate_posns & bit_posn){
      gs->state[row_num][i] = INCORRECT;
    }
    if (num == 0) {
      gs->state[row_num][i] = NONE;
    }
  }

  return (gs->row_is_right[row_num] = (seen_posns == 0b0000001111111110 && duplicate_posns == 0b0000000000000000)); //this is a fully solved row's state
}

INLINE bool column_check (game_state *gs, uint8_t column_num) {
  uint16_t seen_posns = 0;
  uint16_t duplicate_posns = 0;

  for (uint8_t i = 0; i < 9; i++) {
    uint8_t num = gs->nums[i][column_num];
    if (num == 0) {
      continue;
    }
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (seen_posns & bit_posn) {
      duplicate_posns |= bit_posn;
    }
    seen_posns |= bit_posn;
  }

  for (uint8_t i = 0; i < 9; i++) {
    uint8_t num = gs->nums[i][column_num];
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (duplicate_posns & bit_posn) {
      gs->state[i][column_num] = INCORRECT;
    }
    if (num == 0) {
      gs->state[i][column_num] = NONE;
    }
  }

  return (gs->column_is_right[column_num] = (seen_posns == 0b0000001111111110 && duplicate_posns == 0b0000000000000000));
}

INLINE bool box_check (game_state *gs, uint8_t box_num) {
  uint16_t seen_posns = 0;
  uint16_t duplicate_posns = 0;

  for (uint8_t i = 0; i < 9; i++) {
    uint8_t row = (box_num / 3) * 3 + (i / 3);
    uint8_t col = (box_num % 3) * 3 + (i % 3);
    uint8_t num = gs->nums[row][col];
    uint16_t bit_posn = (uint16_t)(1u << num);
    if(seen_posns & bit_posn) {
      duplicate_posns |= bit_posn;
    }
    seen_posns |= bit_posn;
  }

  for (uint8_t i = 0; i < 9; i++) {
    uint8_t row = (box_num / 3) * 3 + (i / 3);
    uint8_t col = (box_num % 3) * 3 + (i % 3);
    uint8_t num = gs->nums[row][col];
    uint16_t bit_posn = (uint16_t)(1u << num);
    if (duplicate_posns & bit_posn) {
      gs->state[row][col] = INCORRECT;
    }
    if (num == 0) {
      gs->state[row][col] = NONE;
    }
  }

  return (gs->box_is_right[box_num] = (seen_posns == 0b0000001111111110 
    && duplicate_posns == 0b0000000000000000));
}

game_state game_loop(game_state gs) {
  memset(gs.state, 0, 81*sizeof(uint8_t));
  uint8_t control = 0;
  for (uint8_t i = 0; i < 9; i++) {
    control += row_check(&gs, i);
  }
  for (uint8_t i = 0; i < 9; i++) {
    control += column_check(&gs, i);
  }
  for (uint8_t i = 0; i < 9; i++) {
    control += box_check(&gs, i);
  }
  if (control == 27) {
    gs.has_won = true;
  }
  
  return gs;
}
