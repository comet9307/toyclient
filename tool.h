#ifndef _TOY_TOOL_H_
#define _TOY_TOOL_H_

#include <sys/types.h>

void toy_print_msg(const char *msg);
void toy_warning(const char *msg);
void toy_kill(const char *msg);
void toy_tool_md5_to_str(const u_char *md5, char *str_buf);
int toy_is_uint(const char *str);

#endif // _TOY_TOOL_H_