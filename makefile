ifeq ($(DEBUG), 1)
    CFLAGS =-DDEBUG
endif

all: ssu_shell pps ttop

ssu_shell: ssu_shell.c
	${CC} ssu_shell.c -o ssu_shell $(CFLAGS)

pps: pps.c
	${CC} pps.c -o pps $(CFLAGS) -lncurses -lm

ttop: ttop.c
	${CC} ttop.c -o ttop $(CFLAGS) -lncurses -lm

clean:
	rm ssu_shell ttop pps
