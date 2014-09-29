#include "command.h"
#include "client.h"
#include "upload.h"
#include "download.h"
#include "select_cycle.h"

#include <unistd.h>

struct toy_select_cycle_s 
{
	fd_set rd_set;
	fd_set wr_set;
	fd_set excep_set;
	int max_fd;

} toy_select_cycle_global;


void toy_selcycle_fdset_add_fd(int fd, enum FDSET_TYPE set_type)
{
	if(set_type == FT_READ)
		FD_SET(fd, &toy_select_cycle_global.rd_set);
	else if(set_type == FT_WRITE)
		FD_SET(fd, &toy_select_cycle_global.wr_set);
	else
		FD_SET(fd, &toy_select_cycle_global.excep_set);

	if(fd > toy_select_cycle_global.max_fd)
		toy_select_cycle_global.max_fd = fd;

	// must judge if the number greater than FD_SETSIZE
}


void toy_selcycle_fdset_rm_fd(int fd, enum FDSET_TYPE set_type)
{
	if(set_type == FT_READ)
		FD_CLR(fd, &toy_select_cycle_global.rd_set);
	else if(set_type == FT_WRITE)
		FD_CLR(fd, &toy_select_cycle_global.wr_set);
	else 
		FD_CLR(fd, &toy_select_cycle_global.excep_set);
}

int toy_selcycle_isset(int fd, enum FDSET_TYPE set_type)
{
	if(set_type == FT_READ)
		return FD_ISSET(fd, &toy_select_cycle_global.rd_set);
	else if(set_type == FT_WRITE)
		return FD_ISSET(fd, &toy_select_cycle_global.wr_set);
	return FD_ISSET(fd, &toy_select_cycle_global.excep_set);
}

void toy_select_cycle()
{
	FD_ZERO(&toy_select_cycle_global.rd_set);
	FD_ZERO(&toy_select_cycle_global.wr_set);
	toy_selcycle_fdset_add_fd(STDIN_FILENO, FT_READ);

	fd_set rdset_tmp;
	fd_set wrset_tmp;
	int fd_ready;
	struct toy_upload_s *uploads_array;
	struct toy_download_s *downloads_array;
	for(;;)
	{
		rdset_tmp = toy_select_cycle_global.rd_set;
		wrset_tmp = toy_select_cycle_global.wr_set;
		fd_ready = select(toy_select_cycle_global.max_fd + 1, &rdset_tmp, &wrset_tmp, 0, 0);
		// never happened
		if(fd_ready == 0)
		{

		}
		else if( fd_ready == -1 )
		{
			// do something on select error
			toy_warning("select error");
		}
		else
		{
			if(FD_ISSET(STDIN_FILENO, &rdset_tmp))
				toy_cmd_handler();
			if(FD_ISSET(toy_client_global.fd, &rdset_tmp))
				toy_client_handler();
			u_int i;
			uploads_array = toy_upload_global_pool.uploads_array;
			for(i = 0; i < toy_upload_global_pool.capacity; ++i)
			{
				if(uploads_array[i].used == 1)
				{
					if(FD_ISSET(uploads_array[i].fd, &rdset_tmp) || FD_ISSET(uploads_array[i].fd, &wrset_tmp))
						toy_upload_handler(uploads_array + i);
				}
			}
			downloads_array = toy_download_global_pool.downloads_array;
			for(i = 0; i < toy_download_global_pool.capacity; ++i)
			{
				if(downloads_array[i].used == 1)
				{
					if(FD_ISSET(downloads_array[i].fd, &rdset_tmp) || FD_ISSET(downloads_array[i].fd, &wrset_tmp))
						toy_download_handler(downloads_array + i);
				}
			}
		}
	}
}