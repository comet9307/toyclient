#ifndef _TOY_DOWNLOAD_H_
#define _TOY_DOWNLOAD_H_

#include <sys/types.h>


struct toy_download_s
{
	enum DOWNLOAD_STATE
	{
		DS_UNCONNECTED,
		DS_SEND_MSG,
		DS_RECV_MSG,
		DS_TRANSFERING
	} state;
	int used;
	int fd;

	char *localname;
	char *dstname;
	int data_file_fd;
	int msg_file_fd;
	u_int down_len;
	u_int file_size;

	u_char *filemsg;
	u_int msg_len;
	u_int done_len;

	u_char *data_buf;
	u_int data_len;
};

struct toy_download_pool_s
{
	u_int downloads_num;
	u_int capacity;
	struct toy_download_s *downloads_array;
};

extern struct toy_download_pool_s toy_download_global_pool;

void toy_download_handler(struct toy_download_s *p_download);
void toy_download_pool_clear();
int toy_download_add_elem(const char *dstname, const char *localname);

#endif  // _TOY_DOWNLOAD_H_