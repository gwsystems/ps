LNAME     = ps
CLIB      = lib$(LNAME).a

CINC      = -I.
CC        = gcc
LD        = $(CC)
MAKE      = make

COPT      = -O3
CWARN     = -Wno-long-long
CFLAGS    = -MD -MP -std=c11 -Wall -Wpedantic -Wextra $(CWARN) $(COPT)
MAKEFLAGS = --no-print-directory --section-alignment 0x1000 -I$(PWD)
