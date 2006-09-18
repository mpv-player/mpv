SRCS = 14_4.c 28_8.c cook.c drv2.c drv3.c drv4.c ra.c rv30.c sipr.c
TARGETS = $(SRCS:.c=.so.6.0)

%.o: %.c
	cc -c $< -g

%.so.6.0: %.o
	ld -shared -o $@ $< -ldl -lc

all: $(TARGETS)

clean:
	rm -f *.so.6.0
