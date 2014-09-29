main:
	gcc md5.c client.c command.c file.c main.c select_cycle.c tool.c upload.c download.c -g -o toyclient  -O0
