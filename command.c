#include "client.h"
#include "tool.h"
#include "md5.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

enum CMD_TYPE
{
	CT_CONNECT,
	CT_REGISTER,
	CT_USER,
	CT_UPLOAD,
	CT_LIST,
	CT_DOWNLOAD,
	CT_LOGOUT,
	CT_DISCONNECT,
	CT_EXIT,
	CT_ERROR
};



void toy_cmd_do_connect(char *cmdstr)
{
	char *ip_start = strstr(cmdstr, "connect") + sizeof("connect");
	while(*ip_start == ' ')
		++ip_start;
	char *ip_end = ip_start;
	u_int ipstr_len = 0;
	while( (*ip_end >= '0' && *ip_end <= '9') || *ip_end == '.')
	{
		++ip_end;
		++ipstr_len;
	}
	char *ipstr = malloc(ipstr_len + 1);
	memcpy(ipstr, ip_start, ipstr_len);
	ipstr[ipstr_len] = 0;

	toy_client_connect(ipstr);

	free(ipstr);
}

void toy_cmd_do_register(char *cmdstr)
{
	char *user_start = strstr(cmdstr, "register") + sizeof("register");
	while(*user_start == ' ')
		++user_start;
	u_int user_len = 0;
	while(  (user_start[user_len] <= 'z' && user_start[user_len] >= 'a')
			|| (user_start[user_len] <= 'Z' && user_start[user_len] >= 'A')
			|| (user_start[user_len] <= '9' && user_start[user_len] >= '0')
			|| user_start[user_len] == '_'  )
		++user_len;
	char *user_name = malloc(user_len + 1);
	memcpy(user_name, user_start, user_len);
	user_name[user_len] = 0;

	const char *pwd_start = user_start + user_len + 1;
	while(*pwd_start == ' ')
		++pwd_start;
	u_int pwd_len = 0;
	while(  (pwd_start[pwd_len] <= 'z' && pwd_start[pwd_len] >= 'a')
			|| (pwd_start[pwd_len] <= 'Z' && pwd_start[pwd_len] >= 'A')
			|| (pwd_start[pwd_len] <= '9' && pwd_start[pwd_len] >= '0')
			|| pwd_start[pwd_len] == '_'  )
		++pwd_len;
	char *pwd = malloc(pwd_len + 1);
	memcpy(pwd, pwd_start, pwd_len);
	pwd[pwd_len] = 0;
	u_char *pwd_hash = ComputeHash(pwd, pwd_len);

	toy_client_register(user_name, pwd_hash);

	free(user_name);
	free(pwd);
}

void toy_cmd_do_user(char *cmdstr)
{
	char *user_start = strstr(cmdstr, "user") + sizeof("user");
	while(*user_start == ' ')
		++user_start;
	u_int user_len = 0;
	while(  (user_start[user_len] <= 'z' && user_start[user_len] >= 'a')
			|| (user_start[user_len] <= 'Z' && user_start[user_len] >= 'A')
			|| (user_start[user_len] <= '9' && user_start[user_len] >= '0')
			|| user_start[user_len] == '_'  )
		++user_len;
	char *user_name = malloc(user_len + 1);
	memcpy(user_name, user_start, user_len);
	user_name[user_len] = 0;

	toy_print_msg("Input Password : ");
	char pwd[50];
	gets(pwd);
	u_int pwd_len = strchr(pwd, 0) - pwd;
	u_char *pwd_hash = ComputeHash(pwd, pwd_len);

	toy_client_login(user_name, pwd_hash);

	free(user_name);
}

void toy_cmd_do_upload(char *cmdstr)
{
	char *localname_start = strstr(cmdstr, "upload") + 7;
	while(*localname_start == ' ')
		++ localname_start;
	u_int loc_len = 0;
	while(localname_start[loc_len] != ' ')
		++ loc_len;

	char *localname = malloc(loc_len + 1);
	memcpy(localname, localname_start, loc_len);
	localname[loc_len] = 0;

	char *dstname_start = localname_start + loc_len + 1;
	while(*dstname_start == ' ' )
		++ dstname_start;
	u_int dst_len = 0;
	while(dstname_start[dst_len] != ' ' && dstname_start[dst_len] != '\t' 
			&& dstname_start[dst_len] != '\n' && dstname_start[dst_len] != 0 )
		++ dst_len;

	char *dstname = malloc(dst_len + 1);
	memcpy(dstname, dstname_start, dst_len);
	dstname[dst_len] = 0;

	toy_client_upload(localname, dstname);

	free(localname);
	free(dstname);
}

void toy_cmd_do_list(char *cmdstr)
{
	char *dir_start = strstr(cmdstr, "list") + sizeof("list");
	while(*dir_start == ' ')
		++dir_start;
	u_int dir_len = 0;
	while(dir_start[dir_len] != ' ' && dir_start[dir_len] != '\t' 
			&& dir_start[dir_len] != '\n' && dir_start[dir_len] != 0 )
		++dir_len;
	char *dir_name = malloc(dir_len + 1);
	memcpy(dir_name, dir_start, dir_len);
	dir_name[dir_len] = 0;

	toy_client_list(dir_name);
	free(dir_name);
}

void toy_cmd_do_download(char *cmdstr)
{
	char *dstname_start = strstr(cmdstr, "download") + 9;
	while(*dstname_start == ' ')
		++ dstname_start;
	u_int dst_len = 0;
	while(dstname_start[dst_len] != ' ')
		++ dst_len;

	char *dstname = malloc(dst_len + 1);
	memcpy(dstname, dstname_start, dst_len);
	dstname[dst_len] = 0;

	char *localname_start = dstname_start + dst_len + 1;
	while(*localname_start == ' ' )
		++ localname_start;
	u_int loc_len = 0;
	while(localname_start[loc_len] != ' ' && localname_start[loc_len] != '\t' 
			&& localname_start[loc_len] != '\n' && localname_start[loc_len] != 0 )
		++ loc_len;

	char *localname = malloc(loc_len + 1);
	memcpy(localname, localname_start, loc_len);
	localname[loc_len] = 0;

	toy_client_download(dstname, localname);

	free(localname);
	free(dstname);
}

void toy_cmd_do_logout()
{
	toy_client_logout();
}

void toy_cmd_do_disconnect()
{
	toy_client_disconnect();
}

void toy_cmd_do_exit()
{
	exit(0);
}

void toy_cmd_do_error()
{
	toy_warning("toy_cmd_do_error()");
}

enum CMD_TYPE toy_cmd_parse(char *cmdstr)
{
	char *cmd_start = cmdstr;
	while(*cmd_start == ' ')
		++cmd_start;
	char *cmd_end = cmd_start;
	while(*cmd_end != ' ' && *cmd_end != '\r' && *cmd_end != '\n' && *cmd_end != 0)
		++cmd_end;
	u_int cmd_len = cmd_end - cmd_start;

	if(cmd_len + 1 == sizeof("connect") && strncmp(cmd_start, "connect", cmd_len) == 0)
		return CT_CONNECT;
	if(cmd_len + 1 == sizeof("register") && strncmp(cmd_start, "register", cmd_len) == 0)
		return CT_REGISTER;
	if(cmd_len + 1 == sizeof("user") && strncmp(cmd_start, "user", cmd_len) == 0)
		return CT_USER;
	if(cmd_len + 1 == sizeof("upload") && strncmp(cmd_start, "upload", cmd_len) == 0)
		return CT_UPLOAD;
	if(cmd_len + 1 == sizeof("download") && strncmp(cmd_start, "download", cmd_len) == 0)
		return CT_DOWNLOAD;
	if(cmd_len + 1 == sizeof("list") && strncmp(cmd_start, "list", cmd_len) == 0)
		return CT_LIST;
	if(cmd_len + 1 == sizeof("logout") && strncmp(cmd_start, "logout", cmd_len) == 0)
		return CT_LOGOUT;
	if(cmd_len + 1 == sizeof("disconnect") && strncmp(cmd_start, "disconnect", cmd_len) == 0)
		return CT_DISCONNECT;
	if(cmd_len + 1 == sizeof("exit") && strncmp(cmd_start, "exit", cmd_len) == 0)
		return CT_EXIT;
	return CT_ERROR;
	
}


void toy_cmd_handler()
{
	char cmdstr[256];
	read(STDIN_FILENO, cmdstr, 256);
	enum CMD_TYPE cmd = toy_cmd_parse(cmdstr);
	if(cmd == CT_CONNECT)
		toy_cmd_do_connect(cmdstr);
	else if(cmd == CT_REGISTER)
		toy_cmd_do_register(cmdstr);
	else if(cmd == CT_USER)
		toy_cmd_do_user(cmdstr);
	else if(cmd == CT_UPLOAD)
		toy_cmd_do_upload(cmdstr);
	else if(cmd == CT_DOWNLOAD)
		toy_cmd_do_download(cmdstr);
	else if(cmd == CT_LIST)
		toy_cmd_do_list(cmdstr);
	else if(cmd == CT_LOGOUT)
		toy_cmd_do_logout();
	else if(cmd == CT_DISCONNECT)
		toy_cmd_do_disconnect();
	else if(cmd == CT_EXIT)
		toy_cmd_do_exit();
	else
		toy_cmd_do_error();
}