#define _POSIX_C_SOURCE 200809L
#include "backend.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LAYOUT_NAME "SoA"

static const uint8_t puzzle[9][9] = {
  {5, 3, 0, 0, 7, 0, 0, 0, 0},
  {6, 0, 0, 1, 9, 5, 0, 0, 0},
  {0, 9, 8, 0, 0, 0, 0, 6, 0},
  {8, 0, 0, 0, 6, 0, 0, 0, 3},
  {4, 0, 0, 8, 0, 3, 0, 0, 1},
  {7, 0, 0, 0, 2, 0, 0, 0, 6},
  {0, 6, 0, 0, 0, 0, 2, 8, 0},
  {0, 0, 0, 4, 1, 9, 0, 0, 5},
  {0, 0, 0, 0, 8, 0, 0, 7, 9},
};

static game_state make_state(int conflicting_board) {
  game_state gs;
  memset(&gs, 0, sizeof gs);
  memcpy(gs.nums, puzzle, sizeof puzzle);

  if (conflicting_board) {
    gs.nums[0][2] = 5;
    gs.nums[1][1] = 6;
    gs.nums[4][4] = 8;
  }

  return gs;
}

static uint64_t checksum_state(const game_state *gs) {
  uint64_t h = UINT64_C(1469598103934665603);

  for (unsigned r = 0; r < 9; ++r) {
    for (unsigned c = 0; c < 9; ++c) {
      h = (h ^ (uint64_t)gs->nums[r][c]) * UINT64_C(1099511628211);
      h = (h ^ (uint64_t)gs->state[r][c]) * UINT64_C(1099511628211);
    }

    h = (h ^ (uint64_t)gs->row_is_right[r]) * UINT64_C(1099511628211);
    h = (h ^ (uint64_t)gs->column_is_right[r]) * UINT64_C(1099511628211);
    h = (h ^ (uint64_t)gs->box_is_right[r]) * UINT64_C(1099511628211);
  }

  h = (h ^ (uint64_t)gs->has_won) * UINT64_C(1099511628211);
  return h;
}

static double elapsed_seconds(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec)
    + (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
}

static void usage(const char *program) {
  fprintf(stderr, "Usage: %s [iterations>0] [valid|conflict]\n", program);
}

int main(int argc, char **argv) {
  uint64_t iterations = UINT64_C(3000000);
  int conflicting_board = 0;

  if (argc >= 2) {
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(argv[1], &end, 10);

    if (argv[1][0] < '0' || argv[1][0] > '9' || *end != '\0' ||
      errno == ERANGE || parsed == 0 || parsed > UINT64_MAX) {
      usage(argv[0]);
      return 2;
    }

    iterations = (uint64_t)parsed;
  }

  if (argc >= 3) {
    if (strcmp(argv[2], "valid") == 0) {
      conflicting_board = 0;
    } else if (strcmp(argv[2], "conflict") == 0) {
      conflicting_board = 1;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  if (argc > 3) {
    usage(argv[0]);
    return 2;
  }

  game_state gs = make_state(conflicting_board);

  const uint64_t warmup =
    iterations < UINT64_C(100000)
      ? iterations / 10 + 1
      : UINT64_C(100000);

  for (uint64_t i = 0; i < warmup; ++i) {
    game_loop(&gs);
  }

  struct timespec start, end;

  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start) != 0) {
    perror("clock_gettime");
    return 1;
  }

  for (uint64_t i = 0; i < iterations; ++i) {
    game_loop(&gs);
  }

  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end) != 0) {
    perror("clock_gettime");
    return 1;
  }

  const double seconds = elapsed_seconds(start, end);
  const double ns_per_call = seconds * 1.0e9 / (double)iterations;
  const uint64_t checksum = checksum_state(&gs);

  printf("RESULT layout=%s mode=%s iterations=%" PRIu64
    " ns_per_call=%.3f checksum=0x%016" PRIx64
    " sizeof_game_state=%zu\n",
    LAYOUT_NAME,
    conflicting_board ? "conflict" : "valid",
    iterations,
    ns_per_call,
    checksum,
    sizeof(game_state));

  return 0;
}
