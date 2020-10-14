#
# example Makefile for Postgres multiple objects extension
#
MYEXT      = mystr
EXTENSION  = $(MYEXT)             # the extensions name
MODULE_big = $(MYEXT)             # the extensions name
DATA       = $(MYEXT)--0.0.1.sql  # script files to install
OBJS       = mystr.o mystr_equal.o

default: all-lib # default just compile for postgres

fun:
	echo "FUN"

# postgres build stuff
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
