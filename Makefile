include Makefile.config
include Makefile.inc

# library
PLATFILE = ps_plat.h
QUIESCEFILE = ps_quiesce.h
CFILES  = $(wildcard *.c) $(wildcard plat/os/$(OSNAME)/*.c) $(wildcard plat/arch/$(ARCHNAME)/*.c) $(wildcard quiesce_type/$(QUIESCETYPE)/*.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))
CDEPS   = $(patsubst %.c,%.d,$(CFILES))
CDEPRM  = $(patsubst %.c,%.d,$(CFILES))

.PHONY: config clean all

all: $(CLIB)

config:
	@arch_res=`cat $(PLATFILE) | grep -w $(ARCHNAME)` || true; \
	osname_res=`cat $(PLATFILE) | grep -w $(OSNAME)` || true; \
	if [ -f $(PLATFILE) -a "$$arch_res" != "" -a "$$osname_res" != "" ]; then \
		exit 0; \
	else \
		rm -f $(PLATFILE); \
		echo '#ifndef PS_PLAT_H'                                        >  $(PLATFILE); \
		echo '#define PS_PLAT_H'                                        >> $(PLATFILE); \
		echo '#include "plat/arch/$(ARCHNAME)/ps_arch.h"'               >> $(PLATFILE); \
		echo '#include "plat/os/$(OSNAME)/ps_os.h"'                     >> $(PLATFILE); \
		echo '#endif	/* PS_PLAT_H */'                                 >> $(PLATFILE); \
	fi 
	
	@quiesce_res=`cat $(QUIESCEFILE) | grep -w $(QUIESCETYPE)` || true; \
	if [ -f $(QUIESCEFILE) -a "$$quiesce_res" != "" ]; then \
		exit 0; \
	else \
		rm -f $(QUIESCEFILE); \
		echo '#ifndef PS_QUIESCE_H'                                     >  $(QUIESCEFILE); \
		echo '#define PS_QUIESCE_H'                                     >> $(QUIESCEFILE); \
		echo '#include "quiesce_type/$(QUIESCETYPE)/ps_quiesce_impl.h"' >> $(QUIESCEFILE); \
		echo '#endif	/* PS_QUIESCE_H */'                              >> $(QUIESCEFILE); \
	fi


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
