
TARGET = mpfc-ce mpfc-kr

LIBS=-lgd -lttf -lpng

all: $(TARGET)

mpfc-ce: mpfc-ce.c
	gcc -Wall -O2 -o $@ $< $(LIBS)

mpfc-kr: mpfc-kr.c
	gcc -Wall -O2 -o $@ $< $(LIBS)

clean:
	-rm -f $(TARGET)
