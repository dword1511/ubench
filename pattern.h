#ifndef UBENCH_PATTERN_H
#define UBENCH_PATTERN_H

extern uint32_t  bench_size;
extern char     *size_pattern;

void print_help(void);
void check_pattern(char *s);
uint32_t get_packet_size(char packet);

#endif /* UBENCH_PATTERN_H */
