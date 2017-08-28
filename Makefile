SRC = ${wildcard *.c}
BIN = ${patsubst %.c, %, $(SRC)}
DEBUG = 0
#make DEBUG=1
all:$(BIN)

CC=gcc


CFLAGS += -I ../libs
CFLAGS += -L ../libs
CFLAGS += -lcjson
CFLAGS += -Wl,-rpath=../libs   #-Wl选项告诉编译器将后面的参数传递给链接器

$(BIN):%:%.c
ifeq ($(DEBUG),0)
	$(CC) -o $@ $^ $(CFLAGS)
else
	$(CC) -o $@ $^ $(CFLAGS) -DDEBUG
endif
	


	
clean:
	$(RM) $(BIN) .*.sw?
