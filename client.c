#include "upload.h"
#include "select_cycle.h"
#include "tool.h"
#include "protocal.h"
#include "client.h"
#include "download.h"

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct toy_client_s toy_client_global;

void toy_client_connect(const char *ipstr)
{
	if(toy_client_global.state != CS_UNCONNECTED)
	{
		toy_warning("have connected! disconnect first!");
		return ;
	}
	int client_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(21);
	serv_addr.sin_addr.s_addr = inet_addr(ipstr);
	if(connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		toy_warning("connect failed");
	else
	{
		toy_client_global.state = CS_CONNECTED;
		toy_client_global.fd = client_fd;
		toy_selcycle_fdset_add_fd(client_fd, FT_READ);
		toy_warning("connect succeed!");
	}
}

void toy_client_logout()
{
	if(toy_client_global.state != CS_LOGINED)
	{
		toy_warning("have no logined yet !");
		return ;
	}
	toy_upload_pool_clear();

	u_char send_buf[1];
	u_int pack_len = sizeof(send_buf);
	send_buf[0] = (u_char)PT_LOGOUT;
	send(toy_client_global.fd, &pack_len, sizeof(pack_len), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);
	toy_client_global.state = CS_CONNECTED;
	toy_warning("logout succeed");
}

void toy_client_disconnect()
{
	if(toy_client_global.state == CS_UNCONNECTED)
	{
		toy_warning("have no connect yet!");
		return ;
	}
	toy_upload_pool_clear();

	toy_selcycle_fdset_rm_fd(toy_client_global.fd, FT_READ);
	close(toy_client_global.fd);
	toy_client_global.state = CS_UNCONNECTED;
	toy_warning("disconnect succeed!");
}

int toy_client_register(const char *user, const char *pwd_hash)
{
	if(toy_client_global.state == CS_UNCONNECTED)
	{
		toy_warning("have not connected!");
		return ;
	}

	u_int pack_len = strlen(user) + 18;
	u_char *send_buf = malloc(pack_len);
	send_buf[0] = (u_char)PT_REGISTER;
	strcpy(send_buf + 1, user);
	memcpy(send_buf + strlen(user) + 2, pwd_hash, 16);
	send(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);
	free(send_buf);

	fd_set rdset;
	FD_ZERO(&rdset);
	FD_SET(toy_client_global.fd, &rdset);
	int maxfd = toy_client_global.fd;
	struct timeval tv = {5, 0};
	int retval = select(maxfd + 1, &rdset, 0, 0, &tv);
	if(retval == 0)
	{
		toy_warning("register time out");
		return;
	}
	if(retval == -1)
	{
		toy_warning("error on select() on toy_client_register()");
		return ;
	}
	if(FD_ISSET(toy_client_global.fd, &rdset))
	{
		u_int recv_len;
		recv(toy_client_global.fd, &recv_len, sizeof(u_int), 0);
		u_char *recv_buf = malloc(recv_len);
		recv(toy_client_global.fd, recv_buf, recv_len, 0);
		if(recv_buf[0] == PT_HAVE_EXISTED)
			toy_warning("user have existed");
		else if(recv_buf[1] = PT_REGISTER_SUCCEED)
			toy_warning("register succeed");
		else
			toy_warning("register wrong");
		free(recv_buf);
	}
}

int toy_client_check_user(const char *user)
{	
	u_int pack_len = strlen(user) + 2;
	u_char *send_buf = malloc(pack_len);
	send_buf[0] = (u_char)PT_USER;
	strcpy(send_buf + 1, user);
	send(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);
	free(send_buf);

	fd_set rdset;
	FD_ZERO(&rdset);
	FD_SET(toy_client_global.fd, &rdset);
	int maxfd = toy_client_global.fd;
	struct timeval tv = {5, 0};			// wait for 10 seconds at most
	int retval = select(maxfd + 1, &rdset, 0, 0, &tv);
	if( retval == 0)
	{
		toy_warning("login time out");
		return 0;
	}
	if(retval == -1)
	{
		toy_warning("some error on select()");
		return 0;
	}
	if(FD_ISSET(toy_client_global.fd, &rdset))
	{
		u_int pack_len;
		recv(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
		u_char *recv_buf = malloc(pack_len);
		recv(toy_client_global.fd, recv_buf, pack_len, 0);
		if(recv_buf[0] == PT_HAVE_LOGINED)
		{
			toy_warning("the user have been logined");
			free(recv_buf);
			return 0;
		}
		if(recv_buf[0] == PT_USER_NOEXIST)
		{
			toy_warning("user no exist");
			free(recv_buf);
			return 0;
		}
		free(recv_buf);
		return 1;
	}
	toy_warning("login error");
	return 0;
}

int toy_client_check_pwdhash(const char *pwd_hash)
{
	u_char pack_len = 17;
	u_char send_buf[17];
	send_buf[0] = (u_char)PT_PWDHASH;
	memcpy(send_buf + 1, pwd_hash, 16);
	send(toy_client_global.fd, &pack_len, sizeof(pack_len), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);

	fd_set rdset;
	FD_ZERO(&rdset);
	FD_SET(toy_client_global.fd, &rdset);
	int maxfd = toy_client_global.fd;
	struct timeval tv = {5, 0};			// wait for 10 seconds at most
	int retval = select(maxfd + 1, &rdset, 0, 0, &tv);
	if( retval == 0)
	{
		toy_warning("login time out");
		return 0;
	}
	if(retval == -1)
	{
		toy_warning("some error on select()");
		return 0;
	}
	if(FD_ISSET(toy_client_global.fd, &rdset))
	{
		u_int pack_len;
		recv(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
		u_char *recv_buf = malloc(pack_len);
		recv(toy_client_global.fd, recv_buf, pack_len, 0);
		if(recv_buf[0] == PT_WRONG_PWD)
		{
			toy_warning("password wrong!");
			free(recv_buf);
			return 0;
		}
		free(recv_buf);
		toy_warning("login succeed!");
		return 1;
	}
	toy_warning("login error");
	return 0;
}

void toy_client_login(const char *user, const char *pwd_hash)
{
	if(toy_client_global.state == CS_UNCONNECTED)
	{
		toy_warning("have not connected!");
		return ;
	}
	if(toy_client_global.state == CS_LOGINED)
	{
		toy_warning("have login already! logout first!");
		return ;
	}

	if(toy_client_check_user(user) == 0)
		return ;
	if(toy_client_check_pwdhash(pwd_hash) == 0)
		return ;
	toy_client_global.state = CS_LOGINED;
}

void toy_client_upload(const char *localname, const char *dstname)
{
	if(toy_client_global.state != CS_LOGINED)
	{
		toy_warning("have not logined!");
		return ;
	}
	int upload_id = toy_upload_add_elem(localname, dstname);
	if(upload_id == -1)
	{
		toy_warning("upload failed!");
		return ;
	}
	u_char send_buf[5];
	send_buf[0] = (u_char)PT_UPLOAD;
	*(u_int*)(send_buf + 1) = upload_id;
	u_int pack_len = sizeof(send_buf);
	send(toy_client_global.fd, &pack_len, sizeof(pack_len), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);
}

void toy_client_on_upload_establish(const u_char *pack_buf)
{
	u_int upload_id = *(u_int*)(pack_buf + 1);
	u_short port = ntohs(*(u_short*)(pack_buf + 5));
	if(toy_upload_connect(upload_id, port) == 0)
		toy_warning("upload connect failed");
}

void toy_client_download(const char *dstname, const char *localname)
{
	if(toy_client_global.state != CS_LOGINED)
	{
		toy_warning("have not logined!");
		return ;
	}
	int download_id = toy_download_add_elem(dstname, localname);
	if(download_id == -1)
	{
		toy_warning("download failed!");
		return ;
	}
	u_char send_buf[5];
	send_buf[0] = (u_char)PT_DOWNLOAD;
	*(u_int*)(send_buf + 1) = download_id;
	u_int pack_len = sizeof(send_buf);
	send(toy_client_global.fd, &pack_len, sizeof(pack_len), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);
}

void toy_client_on_download_establish(const u_char *pack_buf)
{
	u_int download_id = *(u_int*)(pack_buf + 1);
	u_short port = ntohs(*(u_short*)(pack_buf + 5));
	if(toy_download_connect(download_id, port) == 0)
		toy_warning("download connect failed");
}

void toy_client_list(const char *dir_name)
{
	if(toy_client_global.state != CS_LOGINED)
	{
		toy_warning("have not logined!");
		return ;
	}
	u_int pack_len = strlen(dir_name) + 2;
	u_char *send_buf = malloc(pack_len);
	send_buf[0] = (u_char)PT_LIST;
	strcpy(send_buf + 1, dir_name);
	send(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
	send(toy_client_global.fd, send_buf, pack_len, 0);
	free(send_buf);

	fd_set rd_set;
	FD_ZERO(&rd_set);
	FD_SET(toy_client_global.fd, &rd_set);
	int maxfd = toy_client_global.fd;
	struct timeval tv = {20, 0};
	int retval = select(maxfd + 1, &rd_set, 0, 0, &tv);	
	if( retval == 0)
	{
		toy_warning("list time out");
		return ;
	}
	if(retval == -1)
	{
		toy_warning("some error on select() on toy_client_list()");
		return ;
	}
	if(FD_ISSET(toy_client_global.fd, &rd_set))
	{
		u_int pack_len;
		recv(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
		u_char *recv_buf = malloc(pack_len);
		recv(toy_client_global.fd, recv_buf, pack_len, 0);
		recv_buf[pack_len - 1] = 0;
		toy_print_msg(recv_buf);
		free(recv_buf);
		return ;
	}
	toy_warning("list error");

}

enum PACK_TYPE toy_client_parse_pack_type(const u_char *pack_buf)
{
	if(pack_buf[0] == (u_char)PT_UPLOAD_ESTABLISH)
		return PT_UPLOAD_ESTABLISH;
}

void toy_client_handler()
{
	u_int pack_len;
	recv(toy_client_global.fd, &pack_len, sizeof(u_int), 0);
	u_char *pack_buf = malloc(pack_len);
	recv(toy_client_global.fd, pack_buf, pack_len, 0);
	enum PACK_TYPE pk_type = toy_client_parse_pack_type(pack_buf);
	if(pk_type == PT_UPLOAD_ESTABLISH)
		toy_client_on_upload_establish(pack_buf);
	else if(pk_type == PT_DOWNLOAD_ESTABLISH)
		toy_client_on_download_establish(pack_buf);
}