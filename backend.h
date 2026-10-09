#pragma once
#include <stdint.h>

#ifndef COMPILER_FLAGS_H
#define COMPILER_FLAGS_H

#if defined(_MSC_VER)
    #define INLINE static __forceinline

#elif defined(__GNUC__) || defined(__clang__)
    #define INLINE static inline __attribute__((always_inline))

#else
    #define INLINE static inline
#endif


#ifndef __has_c_attribute
    #define __has_c_attribute(x) 0
#endif

#ifndef __has_attribute
    #define __has_attribute(x) 0
#endif
#endif

typedef enum cell_state : uint8_t {
  NONE,
  INCORRECT
} cell_state;

typedef struct cell {
  uint8_t num;
  cell_state state;
} cell;

typedef struct game_state {
  uint8_t nums[9][9];
  cell_state state [9][9];
  bool row_is_right[9];
  bool column_is_right[9];
  bool box_is_right[9];
  bool has_won;
} game_state;


game_state game_loop(game_state gs);
