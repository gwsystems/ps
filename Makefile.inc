LNAME     = ps
CLIB      = lib$(LNAME).a

CINC     += -I. $(CINC_ENV)
CC        = gcc
LD        = $(CC)
MAKE      = make

COPT      = -O3
CWARN     = -Wno-long-long
CFLAGS    = -m32 -MD -MP -std=c11 -Wall -Wpedantic -Wextra $(CWARN) $(COPT) $(CINC) -Wno-unused-function
MAKEFLAGS = --no-print-directory --section-alignment 0x1000 -I$(PWD)
