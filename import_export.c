#include "backend.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int write_to_csv(game_state *gs, const char abs_filepath[]) {
  if (gs == NULL || abs_filepath == NULL) {
    return -1;
  }
  FILE *csv = fopen(abs_filepath, "w");
  if (csv == NULL) {
    return -1;
  }
  for (uint8_t row = 0; row < 9; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      fprintf(csv, "%w8u,", gs->nums[row][col]);
    }
    fprintf(csv, "%w8u\n", gs->nums[row][8]);
  }
  return fclose(csv) == EOF ? -1 : 0;
}

int read_from_csv(game_state *gs, const char abs_filepath[]) {
  if (gs == NULL || abs_filepath == NULL) {
    return -1;
  }

  FILE *csv = fopen(abs_filepath, "r");
  if (csv == NULL) {
    return -1;
  }
  char line[64];

  for (uint8_t row = 0; row < 9; row++) {
    if (fgets(line, sizeof(line), csv) == NULL) {
      memset(gs->nums, 0, 81*sizeof(uint8_t));
      fclose(csv);
      return -1;
    }
    char *token = strtok(line, ",\n\r");

    for (uint8_t col = 0; col < 9; col++) {
      if (token == NULL) {
        memset(gs->nums, 0, 81*sizeof(uint8_t));
        fclose(csv);
        return -1;
      }
      gs->nums[row][col] = (uint8_t)strtoul(token, NULL, 10);
      token = strtok(NULL, ",\n\r");
    }
  }

  if (ferror(csv)) {
    memset(gs->nums, 0, 81*sizeof(uint8_t));
    fclose(csv);
    return -1;
  }
  fclose(csv);
  return 0;
}
