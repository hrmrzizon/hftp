CC = gcc
CFLAGS = -std=c11 -D_XOPEN_SOURCE=700 -pthread -lrt -g
LIBS = 
INCLUDES = -I ./ 
TARGET = test_http test_thread_io hftp

TEST_HTTP_OBJ = test_http.o url.o http_core.o http_method.o http_net_proc.o utilities.o mime_type.o hftp.o
TEST_THREAD_IO_OBJ = test_thread_io.o threads.o url.o http_core.o http_method.o utilities.o mime_type.o hftp.o
HFTP_OBJ = hftp_entry.o threads.o url.o http_core.o http_method.o utilities.o mime_type.o hftp.o

all: $(TARGET) 

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $(INCLUDES) $<

test_http: $(TEST_HTTP_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_HTTP_OBJ) $(INCLUDES)

test_thread_io: $(TEST_THREAD_IO_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_THREAD_IO_OBJ) $(INCLUDES)

hftp: $(HFTP_OBJ)
	$(CC) $(CFLAGS) -o $@ $(HFTP_OBJ) $(INCLUDES)

cleano:
	rm *.o

clean:
	rm *.o
	rm $(TARGET)

