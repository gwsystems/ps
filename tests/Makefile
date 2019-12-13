include ../Makefile.inc

CINC   += -I../
LDFLAGS = -l$(LNAME) -pthread -lnuma -L..

# tests
TCFILES = $(wildcard *.c)
TCOBJS  = $(patsubst %.c,%.o,$(TCFILES))
TCDEPS  = $(patsubst %.c,%.d,$(TCFILES))
EXECS   = $(patsubst %.c,%.test,$(TCFILES))

.PHONY: clean all

all: $(EXECS)

%.o:%.c
	$(CC) $(CINC) $(CFLAGS) -o $@ -c $<

%.test:%.o
	$(LD) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(EXECS) $(TCOBJS) $(TCDEPS)

-include $(TCDEPS)
