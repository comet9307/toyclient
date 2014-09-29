#ifndef _TOY_UPLOAD_H_
#define _TOY_UPLOAD_H_

#include <sys/types.h>

struct toy_upload_s
{
	enum UPLOAD_STATE
	{
		US_UNCONNECTED,
		US_SEND_MSG,
		US_RECV_MSG,
		US_SEND_DATA
	} state;
	int used;
	int fd;

	u_char *filemsg;
	u_int msg_len;

	u_int *upbytes_array;
	u_int blocks_num;
	u_int file_size;

	u_char *data_buf;
	u_int data_len;
	u_int block_ord;
	struct toy_upfile_s 
	{
		int fd;
		pthread_mutex_t mutex;
	} file;

	u_int done_len;

	char *localname;
	char *dstname;
};

struct toy_upload_pool_s 
{
	u_int uploads_num;
	u_int capacity;
	struct toy_upload_s *uploads_array;
};

extern struct toy_upload_pool_s toy_upload_global_pool;

void toy_upload_handler(struct toy_upload_s *p_upload);
void toy_upload_pool_clear();
int toy_upload_add_elem(const char *localname, const char *dstname);

#endif // _TOY_UPLOAD_H_