CC = gcc
CFLAGS  =
CPPFLAGS = -ggdb -g3
LDFLAGS =

all: $(TARGET)


$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.c.o: $(CC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

sendrecv.o: Ether.h
