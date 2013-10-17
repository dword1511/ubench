#ifndef UBENCH_MISC_H
#define UBENCH_MISC_H

void     assemble_data(void);
void     fill_out(int fd, uint16_t size, char *path);
uint64_t calc_us(struct timespec before, struct timespec after);
void     signal_handler(int signum);
int      is_uint(char *s);

uint8_t  *write_buf, *read_buf, *zeros;

#endif /* UBENCH_MISC_H */
