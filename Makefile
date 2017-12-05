include Makefile.config
include Makefile.inc

# library
PLATFILE = ps_plat.h
CFILES  = $(wildcard *.c) $(wildcard plat/os/$(OSNAME)/*.c) $(wildcard plat/arch/$(ARCHNAME)/*.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))
CDEPS   = $(patsubst %.c,%.d,$(CFILES))
CDEPRM  = $(patsubst %.c,%.d,$(CFILES))

.PHONY: config clean all

all: $(CLIB)

config:
	@rm -f $(PLATFILE)
	@echo '#ifndef PS_PLAT_H'                          >  $(PLATFILE)
	@echo '#define PS_PLAT_H'                          >> $(PLATFILE)
	@echo '#include "plat/arch/$(ARCHNAME)/ps_arch.h"' >> $(PLATFILE)
	@echo '#include "plat/os/$(OSNAME)/ps_os.h"'       >> $(PLATFILE)
	@echo '#endif	/* PS_PLAT_H */'                   >> $(PLATFILE)

$(PLATFILE): config

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(CLIB):$(PLATFILE) $(COBJS)
	$(AR) cr $@ $^

tests: $(CLIB)
	$(MAKE) $(MAKEFLAGS) -C tests/ all

clean:
	rm -f $(PLATFILE) $(COBJS) $(CLIB) $(CDEPRM)
	$(MAKE) $(MAKEFLAGS) -C tests/ clean

-include $(CDEPS)
