#ifndef _TOY_CLIENT_H_
#define _TOY_CLIENT_H_

struct toy_client_s
{
	int fd;
	enum CLIENT_STATE
	{
		CS_UNCONNECTED,
		CS_CONNECTED,
		CS_LOGINED
	} state;
} ;

void toy_client_handler();
void toy_client_connect(const char *ipstr);
int toy_client_register(const char *user, const char *pwd_hash);
void toy_client_logout();
void toy_client_disconnect();
void toy_client_login(const char *user, const char *pwd_hash);
void toy_client_upload(const char *localname, const char *dstname);
void toy_client_download(const char *dstname, const char *localname);
void toy_client_list(const char *dir_name);

extern struct toy_client_s toy_client_global;

#endif // _TOY_CLIENT_H_