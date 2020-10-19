#
# Makefile for Postgres pgbdd extension and test environment
#
CC         = CC
MYEXT      = pgbdd
EXTENSION  = $(MYEXT)
MODULE_big = $(MYEXT)
DATA       = pgbdd--0.0.1.sql

SRCBASE    = TAG_dictionary.c TAG_bdd.c TAG_utils.c TAG_vector.c

OBJS       = $(SRCBASE:TAG_%.c=pg_%.o)

# default: all-lib # default just compile for postgres
default: run

TEST-PACKAGE = test_package
TEST-OBJECTS = $(SRCBASE:TAG_%.c=test_%.o)

HEADERS      = pg_config.h bdd.h dictionary.h utils.h vector.h

run:    $(TEST-PACKAGE)
	# lldb ./$(TEST-PACKAGE)
	./$(TEST-PACKAGE)

$(TEST-PACKAGE): test_config.h $(TEST-PACKAGE).o $(TEST-OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(TEST-PACKAGE).o $(TEST-OBJECTS)

clean: test-clean

test-clean: 
	$(RM) $(TEST-PACKAGE) $(TEST-PACKAGE).o $(TEST-OBJECTS) $(PG-OBJECTS)

backup: 
	tar cvf - /Users/flokstra/Github/DuBio | gzip > /Users/flokstra/backup/DuBio.tar.gz
	scp /Users/flokstra/backup/DuBio.tar.gz farm01.ewi.utwente.nl:

pg_bdd.o : bdd.c
pg_dictionary.o : dictionary.c
pg_utils.o : utils.c
pg_vector.o : vector.c

test_bdd.o : bdd.c
test_dictionary.o : dictionary.c
test_utils.o : utils.c
test_vector.o : vector.c

# postgres build stuff
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
