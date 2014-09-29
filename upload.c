#include "md5.h"
#include "select_cycle.h"
#include "client.h"
#include "tool.h"
#include "file.h"
#include "protocal.h"
#include "upload.h"

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


#define BUF_SIZE				2048


struct toy_upload_pool_s toy_upload_global_pool;
void toy_upload_abort(struct toy_upload_s *p_upload);


void toy_upload_rm_elem(struct toy_upload_s *p_upload);

void init_upload_s(struct toy_upload_s *ptr)
{	
	ptr->state = US_UNCONNECTED;
	ptr->used = 1;
	ptr->fd = -1;
	ptr->filemsg = NULL;
	ptr->msg_len = 0;
	ptr->upbytes_array = NULL;	

	ptr->data_buf = NULL;
	ptr->data_len = 0;
	ptr->block_ord = 0;

	ptr->done_len = 0;

	ptr->file.fd = -1;
	ptr->blocks_num = 0;
	ptr->localname = NULL;
	ptr->dstname = NULL;
}

void toy_upload_pool_adjust()
{
	u_int old_capacity = toy_upload_global_pool.capacity;
	u_int new_capacity = old_capacity + 25;
	struct toy_upload_s *ptr = malloc(new_capacity * sizeof(struct toy_upload_s));
	memset(ptr, 0, new_capacity * sizeof(struct toy_upload_s));			//be  sure that member 'used' to be 0
	memcpy(ptr, toy_upload_global_pool.uploads_array, old_capacity * sizeof(struct toy_upload_s));
	free(toy_upload_global_pool.uploads_array);
	toy_upload_global_pool.uploads_array = ptr;
	toy_upload_global_pool.capacity = new_capacity;
}

void toy_upload_pool_clear()
{
	u_int i;
	for(i = 0; i < toy_upload_global_pool.capacity; ++i)
	{
		if(toy_upload_global_pool.uploads_array[i].used == 1)
			toy_upload_rm_elem(toy_upload_global_pool.uploads_array + i);
	}
}

int toy_upload_add_elem(const char *localname, const char *dstname)
{
	if(toy_upload_global_pool.uploads_array == NULL)
	{
		toy_upload_global_pool.uploads_array = malloc(25 * sizeof(struct toy_upload_s));
		memset(toy_upload_global_pool.uploads_array, 0, 25 * sizeof(struct toy_upload_s));
		toy_upload_global_pool.capacity = 25;
	}
	int id;
	for(id = 0; id < toy_upload_global_pool.capacity; ++id)
	{
		if(toy_upload_global_pool.uploads_array[id].used == 0)
			break;
	}
	if(id == toy_upload_global_pool.capacity)
		toy_upload_pool_adjust();


	struct toy_upload_s *ptr = toy_upload_global_pool.uploads_array + id;
	init_upload_s(ptr);

	ptr->file.fd = toy_file_open(localname, O_RDWR);
	if(ptr->file.fd == -1)
	{
		toy_warning("open file failed");
		toy_upload_abort(ptr);
		return -1;
	}
	pthread_mutex_init(&ptr->file.mutex, NULL);
	
	if( toy_file_getsize(localname, &ptr->file_size) == 0)
	{
		toy_warning("open file failed");
		toy_upload_abort(ptr);
		return -1;
	}
	ptr->blocks_num = (ptr->file_size - 1) / BLOCK_SIZE + 1;

	ptr->localname = malloc(strlen(localname) + 1);
	strcpy(ptr->localname, localname);
	ptr->dstname = malloc(strlen(dstname) + 1);
	strcpy(ptr->dstname, dstname);

	++toy_upload_global_pool.uploads_num;

	return id;
}

void toy_upload_rm_elem(struct toy_upload_s *p_upload)
{
	if(p_upload->used == 0)
		return ;
	p_upload->used = 0;

	if(p_upload->fd != -1)
	{
		if(toy_selcycle_isset(p_upload->fd, FT_READ))
			toy_selcycle_fdset_rm_fd(p_upload->fd, FT_READ);
		if(toy_selcycle_isset(p_upload->fd, FT_WRITE))
			toy_selcycle_fdset_rm_fd(p_upload->fd, FT_WRITE);
		close(p_upload->fd);
	}

	if(p_upload->localname)
		free(p_upload->localname);
	if(p_upload->dstname)
		free(p_upload->dstname);

	pthread_mutex_destroy(&p_upload->file.mutex);
	if(p_upload->file.fd != -1)
		toy_file_close(p_upload->file.fd);
	
	if(p_upload->filemsg)
		free(p_upload->filemsg);
	if(p_upload->data_buf)
		free(p_upload->data_buf);
	if(p_upload->upbytes_array)
		free(p_upload->upbytes_array);

	--toy_upload_global_pool.uploads_num;
}

void toy_upload_done(struct toy_upload_s *p_upload)
{
	toy_warning("upload done");
	toy_upload_rm_elem(p_upload);
}

void toy_upload_abort(struct toy_upload_s *p_upload)
{
	toy_warning("upload abort");
	toy_upload_rm_elem(p_upload);
}

void toy_upload_send_filemsg(struct toy_upload_s *p_upload)
{
	if(p_upload->done_len == 0)
		send(p_upload->fd, &p_upload->msg_len, sizeof(u_int), 0);
	
	u_int remain_len = p_upload->msg_len - p_upload->done_len;
	u_int send_len = send(p_upload->fd, p_upload->filemsg + p_upload->done_len, remain_len, 0);
	p_upload->done_len += send_len;
	if(p_upload->done_len == p_upload->msg_len)
	{
		p_upload->state = US_RECV_MSG;
		p_upload->done_len = 0;
		free(p_upload->filemsg);
		p_upload->filemsg = NULL;
		p_upload->msg_len = 0;
		p_upload->upbytes_array = malloc(p_upload->blocks_num * 4);
		toy_selcycle_fdset_rm_fd(p_upload->fd, FT_WRITE);
		toy_selcycle_fdset_add_fd(p_upload->fd, FT_READ);
	}
}

void toy_upload_recv_filemsg(struct toy_upload_s *p_upload)
{
	u_int remain_len = p_upload->blocks_num * 4 - p_upload->done_len;
	u_int recv_len = recv(p_upload->fd, (u_char*)p_upload->upbytes_array + p_upload->done_len, remain_len, 0);
	p_upload->done_len += recv_len;
	if(p_upload->done_len == p_upload->blocks_num * 4)
	{
		if(toy_upload_if_done(p_upload))
		{
			toy_upload_done(p_upload);
			return;
		}
		p_upload->state = US_SEND_DATA;
		p_upload->done_len = 0;
		p_upload->block_ord = 0;
		p_upload->data_buf = malloc(BUF_SIZE);

		p_upload->data_len = 0;		// must set to 0
		toy_selcycle_fdset_rm_fd(p_upload->fd, FT_READ);
		toy_selcycle_fdset_add_fd(p_upload->fd, FT_WRITE);
	}
}
	
int toy_upload_get_filemsg(struct toy_upload_s *p_upload)
{
	char *localname = p_upload->localname;
	char *dstname = p_upload->dstname;
	int fd = p_upload->fd;
	u_int dstname_len = strlen(dstname);

	p_upload->msg_len = p_upload->blocks_num  * 16 + dstname_len + 9;
	p_upload->filemsg = malloc(p_upload->msg_len);
	strcpy(p_upload->filemsg, dstname);
	*(u_int*)(p_upload->filemsg + dstname_len + 1) = p_upload->blocks_num;
	*(u_int*)(p_upload->filemsg + dstname_len + 5) = p_upload->file_size;
	if( toy_file_get_blocks_md5(localname, p_upload->filemsg + dstname_len + 9, BLOCK_SIZE) == 0)
	{
		toy_warning("get filemsg failed");
		return 0;
	}
	toy_warning("calculate md5 done");
	return 1;
}

int toy_upload_connect(u_int upload_id, u_short port)
{
	int upload_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	getpeername(toy_client_global.fd, (struct sockaddr*)&serv_addr, &addr_len);
	serv_addr.sin_port = htons(port);
	if(connect(upload_fd, (struct sockaddr*)&serv_addr, addr_len) == -1)
		return 0;
	toy_upload_global_pool.uploads_array[upload_id].fd = upload_fd;

	struct toy_upload_s *p_upload = toy_upload_global_pool.uploads_array + upload_id;
	p_upload->state = US_SEND_MSG;

	if(toy_upload_get_filemsg(p_upload) == 0)
	{
		toy_upload_abort(p_upload);
		return 0;
	}
	p_upload->done_len = 0;

	toy_selcycle_fdset_add_fd(p_upload->fd, FT_WRITE);
	return 1;
}


u_int toy_upload_get_block_size(struct toy_upload_s *p_upload, u_int block_ord)
{
	if(block_ord == p_upload->blocks_num - 1)
		return p_upload->file_size - block_ord * BLOCK_SIZE;
	return BLOCK_SIZE;
}


void toy_upload_ready_data(struct toy_upload_s *p_upload)
{
	if(p_upload->data_len)
		return ;

	u_int block_ord = p_upload->block_ord;
	u_int upbytes = p_upload->upbytes_array[block_ord];

	u_int remain_bytes = toy_upload_get_block_size(p_upload, block_ord) - upbytes;
	while(remain_bytes == 0)
	{
		++p_upload->block_ord;
		block_ord = p_upload->block_ord;
		upbytes = p_upload->upbytes_array[block_ord];
		remain_bytes = toy_upload_get_block_size(p_upload, block_ord) - upbytes;
	}
	u_int load_size;
	if(remain_bytes < BUF_SIZE)
		load_size = remain_bytes;
	else
		load_size = BUF_SIZE;
	u_int offset = block_ord * BLOCK_SIZE + upbytes;
	pthread_mutex_lock(&p_upload->file.mutex);
	toy_file_read(p_upload->file.fd, p_upload->data_buf, load_size, offset);
	pthread_mutex_unlock(&p_upload->file.mutex);

	p_upload->done_len = 0;
	p_upload->data_len = load_size;
}

/*
int toy_upload_if_done(struct toy_upload_s *p_upload)
{
	if(p_upload->block_ord == p_upload->blocks_num - 1)
	{
		u_int block_size = p_upload->file_size - p_upload->block_ord * BLOCK_SIZE;
		u_int upbytes = p_upload->upbytes_array[p_upload->block_ord];
		if(block_size == upbytes)
			return 1;
	}
	return 0;
}*/

int toy_upload_if_done(struct toy_upload_s *p_upload)
{
	u_int block_ord = p_upload->block_ord;
	if(block_ord == 0)
	{
		while(block_ord < p_upload->blocks_num - 1)
		{
			if(p_upload->upbytes_array[block_ord] < BLOCK_SIZE)
				return 0;
			++ block_ord;
		}
	}
	if(block_ord == p_upload->blocks_num - 1)
	{
		u_int block_size = p_upload->file_size - block_ord * BLOCK_SIZE;
		u_int upbytes = p_upload->upbytes_array[block_ord];
		if(block_size == upbytes)
			return 1;
	}
	return 0;
}

void toy_upload_send_data(struct toy_upload_s *p_upload)
{
	u_int offset;
	u_int len;
	toy_upload_ready_data(p_upload);
	u_int send_len = send(p_upload->fd, p_upload->data_buf + p_upload->done_len, p_upload->data_len, 0);
	p_upload->upbytes_array[p_upload->block_ord] += send_len;
	p_upload->done_len += send_len;
	p_upload->data_len -= send_len;
	if(toy_upload_if_done(p_upload))
		toy_upload_done(p_upload);
}


void toy_upload_handler(struct toy_upload_s *p_upload)
{
	if(p_upload->state == US_SEND_MSG)
		toy_upload_send_filemsg(p_upload);
	else if(p_upload->state == US_RECV_MSG)
		toy_upload_recv_filemsg(p_upload);
	else if(p_upload->state == US_SEND_DATA)
		toy_upload_send_data(p_upload);

	// no here
	else
		toy_warning("something wrong");
}