#include "md5.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int toy_file_getsize(const char *filepath, u_int *size_buf)
{
	struct stat stat_buf;
	if( stat(filepath, &stat_buf) == -1)
		return 0;
	*size_buf = stat_buf.st_size;
	return 1;
}


int toy_file_get_blocks_md5(const char *filepath, u_char *md5_buf, u_int block_size)
{
	int file_fd = open(filepath, O_RDONLY);
	if(file_fd == -1)
	{
		toy_warning("open file failed");
		return 0;
	}
	u_int file_size;
	toy_file_getsize(filepath, &file_size) ;
	u_int blocks_num = (file_size - 1) / block_size + 1;
	u_int last_block_size = file_size - block_size * (blocks_num - 1);

	u_char md5[16];
	MD5_CTX md5_context;
	u_int read_bytes;
	u_int remain_bytes;
	u_int buf_size = 4096 * 2;
	u_char *tmp_data = malloc(buf_size);
	
	u_int i;
	for(i = 0; i < blocks_num; ++i)
	{
		MD5Init(&md5_context);

		remain_bytes = block_size;
		if(i == blocks_num - 1)
			remain_bytes = last_block_size;
		while(remain_bytes > 0)
		{
			if(remain_bytes > buf_size)
				read_bytes = read(file_fd, tmp_data, buf_size);
			else
				read_bytes = read(file_fd, tmp_data, remain_bytes);
			remain_bytes -= read_bytes;
			MD5Update(&md5_context, tmp_data, read_bytes);
		}
		MD5Final(md5, &md5_context);
		memcpy(md5_buf + i * 16, md5, 16);
	}

	free(tmp_data);
	return 1;
}


int toy_file_read(int fd, void *buf, u_int size, u_int offset)
{
	lseek(fd, offset, SEEK_SET);
	return read(fd, buf, size);
}

int toy_file_write(int fd, void *buf, u_int size, u_int offset)
{
	lseek(fd, offset, SEEK_SET);
	return write(fd, buf, size);
}

int toy_file_open(const char *pathname, int flags)
{
	return open(pathname, flags);
}

int toy_file_remove(const char *pathname)
{
	return remove(pathname);
}

int toy_file_close(int fd)
{
	return close(fd);
}