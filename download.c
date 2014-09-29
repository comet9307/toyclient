#include "md5.h"
#include "select_cycle.h"
#include "client.h"
#include "tool.h"
#include "file.h"
#include "protocal.h"
#include "download.h"

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


#define BUF_SIZE				2048

struct toy_download_pool_s toy_download_global_pool;

void toy_download_rm_elem(struct toy_download_s *p_download);

void init_download_s(struct toy_download_s *ptr)
{	
	ptr->state = DS_UNCONNECTED;
	ptr->used = 1;
	ptr->fd = -1;

	ptr->localname = NULL;
	ptr->dstname = NULL;
	ptr->data_file_fd = -1;
	ptr->msg_file_fd = -1;
	ptr->down_len = -1;
	ptr->file_size = -1;

	ptr->filemsg = NULL;
	ptr->msg_len = 0;
	ptr->done_len = 0;

	ptr->data_buf = NULL;
	ptr->data_len = 0;

}

void toy_download_pool_clear()
{
	u_int i;
	for(i = 0; i < toy_download_global_pool.capacity; ++i)
	{
		if(toy_download_global_pool.downloads_array[i].used == 1)
			toy_download_rm_elem(toy_download_global_pool.downloads_array + i);
	}
}

void toy_download_pool_adjust()
{
	u_int old_capacity = toy_download_global_pool.capacity;
	u_int new_capacity = old_capacity + 25;
	struct toy_download_s *ptr = malloc(new_capacity * sizeof(struct toy_download_s));
	memset(ptr, 0, new_capacity * sizeof(struct toy_download_s));			//be  sure that member 'used' to be 0
	memcpy(ptr, toy_download_global_pool.downloads_array, old_capacity * sizeof(struct toy_download_s));
	free(toy_download_global_pool.downloads_array);
	toy_download_global_pool.downloads_array = ptr;
	toy_download_global_pool.capacity = new_capacity;
}

int toy_download_add_elem(const char *dstname, const char *localname)
{
	if(toy_download_global_pool.downloads_array == NULL)
	{
		toy_download_global_pool.downloads_array = malloc(25 * sizeof(struct toy_download_s));
		memset(toy_download_global_pool.downloads_array, 0, 25 * sizeof(struct toy_download_s));
		toy_download_global_pool.capacity = 25;
	}
	int id;
	for(id = 0; id < toy_download_global_pool.capacity; ++id)
	{
		if(toy_download_global_pool.downloads_array[id].used == 0)
			break;
	}
	if(id == toy_download_global_pool.capacity)
		toy_download_pool_adjust();


	struct toy_download_s *ptr = toy_download_global_pool.downloads_array + id;
	init_download_s(ptr);

	char *data_file_name = malloc(strlen(localname) + 1);
	strcpy(data_file_name, localname);
	char *msg_file_name = malloc(strlen(localname) + sizeof("msg"));
	strcpy(msg_file_name, localname);
	strcat(msg_file_name, "msg");
	ptr->data_file_fd = toy_file_open(data_file_name, O_RDWR);
	ptr->msg_file_fd = toy_file_open(msg_file_name, O_RDWR);
	if(ptr->data_file_fd == -1  ||  ptr->msg_file_fd == -1)
	{
		toy_file_remove(data_file_name);
		toy_file_remove(msg_file_name);
		ptr->data_file_fd = toy_file_open(data_file_name, O_RDWR | O_CREAT);
		ptr->msg_file_fd = toy_file_open(msg_file_name, O_RDWR | O_CREAT);
		ptr->down_len = 0;
		ptr->file_size = 0;
		toy_file_write(ptr->msg_file_fd, &ptr->down_len, sizeof(u_int), 0);
	}
	else
	{
		toy_file_read(ptr->msg_file_fd, &ptr->down_len, sizeof(u_int), 0);
		toy_file_read(ptr->msg_file_fd, &ptr->file_size, sizeof(u_int), 4);
	}

	ptr->localname = malloc(strlen(localname) + 1);
	strcpy(ptr->localname, localname);
	ptr->dstname = malloc(strlen(dstname) + 1);
	strcpy(ptr->dstname, dstname);

	++toy_download_global_pool.downloads_num;

	return id;
}

void toy_download_rm_elem(struct toy_download_s *p_download)
{
	if(p_download->used == 0)
		return ;
	p_download->used = 0;

	if(p_download->fd != -1)
	{
		if(toy_selcycle_isset(p_download->fd, FT_READ))
			toy_selcycle_fdset_rm_fd(p_download->fd, FT_READ);
		if(toy_selcycle_isset(p_download->fd, FT_WRITE))
			toy_selcycle_fdset_rm_fd(p_download->fd, FT_WRITE);
		close(p_download->fd);
	}

	if(p_download->localname)
		free(p_download->localname);
	if(p_download->dstname)
		free(p_download->dstname);
	
	if(p_download->filemsg)
		free(p_download->filemsg);
	if(p_download->data_buf)
		free(p_download->data_buf);

	if(p_download->msg_file_fd != -1)
		toy_file_close(p_download->msg_file_fd);
	if(p_download->data_file_fd != -1)
		toy_file_close(p_download->data_file_fd);

	--toy_download_global_pool.downloads_num;
}

void toy_download_done(struct toy_download_s *p_download)
{
	toy_warning("download done");

	if(p_download->msg_file_fd != -1)
		toy_file_close(p_download->msg_file_fd);
	if(p_download->data_file_fd != -1)
		toy_file_close(p_download->data_file_fd);

	char *msg_file_name = malloc(strlen(p_download->localname) + sizeof("msg"));
	strcpy(msg_file_name, p_download->localname);
	strcat(msg_file_name, "msg");
	toy_file_remove(msg_file_name);
	free(msg_file_name);

	toy_download_rm_elem(p_download);
}

void toy_download_abort(struct toy_download_s *p_download)
{
	toy_warning("download abort");
	toy_download_rm_elem(p_download);
}

void toy_download_recv_filemsg(struct toy_download_s *p_download)
{
	if(p_download->done_len == 0)
	{
		recv(p_download->fd, &p_download->msg_len, sizeof(u_int), 0);
		p_download->filemsg = malloc(p_download->msg_len);
		p_download->done_len = 0;
	}

	u_int remain_len = p_download->msg_len - p_download->done_len;
	u_int recv_len = recv(p_download->fd, p_download->filemsg + p_download->done_len, remain_len, 0);
	p_download->done_len += recv_len;

	if(p_download->done_len == p_download->msg_len)
	{
		p_download->down_len = *(u_int*)p_download->filemsg;
		p_download->file_size = *(u_int*)(p_download->filemsg + 4);
		toy_file_write(p_download->msg_file_fd, p_download->filemsg, p_download->msg_len, 0);

		p_download->state = DS_TRANSFERING;

		p_download->data_buf = malloc(BUF_SIZE);
		p_download->data_len = 0;
	}
}

void toy_download_send_filemsg(struct toy_download_s *p_download)
{
	if(p_download->done_len == 0)
		send(p_download->fd, &p_download->msg_len, sizeof(u_int), 0);
	
	u_int remain_len = p_download->msg_len - p_download->done_len;
	u_int send_len = send(p_download->fd, p_download->filemsg + p_download->done_len, remain_len, 0);
	p_download->done_len += send_len;
	if(p_download->done_len == p_download->msg_len)
	{
		p_download->state = DS_RECV_MSG;
		p_download->done_len = 0;
		free(p_download->filemsg);
		p_download->filemsg = NULL;
		p_download->msg_len = 0;
		toy_selcycle_fdset_rm_fd(p_download->fd, FT_WRITE);
		toy_selcycle_fdset_add_fd(p_download->fd, FT_READ);
	}
}

int toy_download_get_filemsg(struct toy_download_s *p_download)
{
	char *dstname = p_download->dstname;
	int fd = p_download->fd;
	u_int dstname_len = strlen(dstname);

	if(p_download->down_len != 0)
	{
		u_int for_num = (p_download->down_len - 1) / BLOCK_SIZE + 1;
		p_download->msg_len = for_num * 16 + dstname_len + 5;
		p_download->filemsg = malloc(p_download->msg_len);
		strcpy(p_download->filemsg, dstname);
		*(u_int*)(p_download->filemsg + dstname_len + 1) = p_download->down_len;
		toy_file_read(p_download->msg_file_fd, p_download->filemsg + dstname_len + 5, p_download->msg_len, 8);
	}
	else
	{
		p_download->msg_len = dstname_len + 5;
		p_download->filemsg = malloc(p_download->msg_len);
		strcpy(p_download->filemsg, dstname);
		*(u_int*)(p_download->filemsg + dstname_len + 1) = p_download->down_len;
	}
	return 1;
}

int toy_download_connect(u_int download_id, u_short port)
{
	int download_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	getpeername(toy_client_global.fd, (struct sockaddr*)&serv_addr, &addr_len);
	serv_addr.sin_port = htons(port);
	if(connect(download_fd, (struct sockaddr*)&serv_addr, addr_len) == -1)
		return 0;
	toy_download_global_pool.downloads_array[download_id].fd = download_fd;

	struct toy_download_s *p_download = toy_download_global_pool.downloads_array + download_id;
	p_download->state = DS_SEND_MSG;

	if(toy_download_get_filemsg(p_download) == 0)
	{
		toy_download_abort(p_download);
		return 0;
	}

	p_download->done_len = 0;

	toy_selcycle_fdset_add_fd(p_download->fd, FT_WRITE);
}

void toy_download_update_tmp_file(struct toy_download_s *p_download)
{
	toy_file_write(p_download->data_file_fd, p_download->data_buf, p_download->data_len, p_download->down_len);
	p_download->down_len += p_download->data_len;
	p_download->data_len = 0;
	toy_file_write(p_download->msg_file_fd, &p_download->down_len, sizeof(u_int), 0);
}

void toy_download_transfer(struct toy_download_s *p_download)
{
	u_int recv_len = recv(p_download->fd, p_download->data_buf + p_download->data_len, BUF_SIZE - p_download->data_len, 0);
	p_download->data_len += recv_len;

	if(recv_len == 0)
	{
		toy_download_abort(p_download);
		return ;
	}
	if(p_download->data_len + p_download->down_len == p_download->file_size)
	{
		toy_download_update_tmp_file(p_download);
		toy_download_done(p_download);
		return ;
	}
	if(p_download->data_len == BUF_SIZE)
		toy_download_update_tmp_file(p_download);
}


void toy_download_handler(struct toy_download_s *p_download)
{
	if(p_download->state == DS_SEND_MSG)
		toy_download_send_filemsg(p_download);
	else if(p_download->state == DS_RECV_MSG)
		toy_download_recv_filemsg(p_download);
	else if(p_download->state == DS_TRANSFERING)
		toy_download_transfer(p_download);
}