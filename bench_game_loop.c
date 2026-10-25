#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include "backend.h"

static inline long long calculate_ns(struct timespec start, struct timespec end) {
    return ((end.tv_sec - start.tv_sec) * 1000000000LL) + (end.tv_nsec - start.tv_nsec);
}

int main(void) {
    struct timespec ts0, ts1, ts2, ts3;

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts0);
    game_state solved_puzzle = {0};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts1);

    if (read_from_csv(&solved_puzzle, "./testing/test_boards/solved_board.csv") != 0) {
        fprintf(stderr, "Error: Failed to read CSV file.\n");
        return -1;
    }
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts2);

    game_loop(solved_puzzle);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts3);

    long long init_ns  = calculate_ns(ts0, ts1);
    long long csv_ns   = calculate_ns(ts1, ts2);
    long long loop_ns  = calculate_ns(ts2, ts3);
    long long total_ns = init_ns + csv_ns + loop_ns;

    printf(" Struct Init: %lld ns\n", init_ns);
    printf(" CSV Reading: %lld ns (%.3f ms)\n", csv_ns, csv_ns / 1000000.0);
    printf(" Game Loop:   %lld ns\n", loop_ns);
    printf("total CPU: %lld ns (%.3f ms)\n", total_ns, total_ns / 1000000.0);

    return 0;
}
