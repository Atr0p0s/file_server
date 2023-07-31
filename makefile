SRCS  :=	file_server.c \
			file_client.c \
			error_functions.c \
			inet_sockets.c \
			client_tools.c
OBJS := $(SRCS:%.c=%.o)

#Compiler flags
CC := gcc
INCL := -I./includes

%.o: %.c
	$(CC) $(INCL) -c $< -o $@

.PHONY: build clean client run_client run_server

build: server.out client.out
server.out: file_server.c error_functions.c inet_sockets.c client_tools.c
	$(CC) file_server.c error_functions.c inet_sockets.c $(INCL) -o $@
client.out: file_client.c error_functions.c inet_sockets.c client_tools.c
	$(CC) file_client.c error_functions.c inet_sockets.c client_tools.c $(INCL) -o $@

clean: 
	rm server.out client.out

client: client.out

run_client:
	./client.out

run_server:
	./server.out