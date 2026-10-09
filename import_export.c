#include "backend.h"
#include <stdio.h>

int write_to_csv(game_state gs, const char filename[]) {
  FILE *csv = fopen(filename, "w");
  if (csv == NULL) {
    return -1;
  }
  return 0;
}
