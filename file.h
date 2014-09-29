#ifndef _TOY_FILE_H_
#define _TOY_FILE_H_


int toy_file_read(int fd, void *buf, u_int size, u_int offset);
int toy_file_write(int fd, void *buf, u_int size, u_int offset);
void toy_file_close(int fd);
int toy_file_open(const char *pathname, int flags);
int toy_file_remove(const char *pathname);
int toy_file_getsize(const char *filepath, u_int *size_buf);
int toy_file_get_blocks_md5(const char *filepath, u_char *md5_buf, u_int block_size);


#endif // _TOY_FILE_H_