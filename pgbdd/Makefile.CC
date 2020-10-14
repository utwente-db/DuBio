CC = cc
CFLAGS = -g

RUN = test_package

HEADERS      = bdd.h dictionary.h utils.h vector.h
PG-OBJECTS   = pg_bdd.o   pg_dictionary.o   pg_utils.o   pg_vector.o
TEST-OBJECTS = test_bdd.o test_dictionary.o test_utils.o test_vector.o

%.o: %.c $(HEADERS)
	$(CC) $< $(CFLAGS) -c -o $@
     
run:    $(RUN)
	# lldb ./$(RUN)
	./$(RUN)

test_package: test_config.h test_package.o $(TEST-OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ test_package.o $(TEST-OBJECTS)

clean: 
	$(RM) test_package.o $(TEST-OBJECTS) $(PG-OBJECTS)

backup: 
	tar cvf - /Users/flokstra/pgext | gzip > /Users/flokstra/backup/pgext.tar.gz
	scp /Users/flokstra/backup/pgext.tar.gz farm01.ewi.utwente.nl:

pg_bdd.o : bdd.c
pg_dictionary.o : dictionary.c
pg_utils.o : utils.c
pg_vector.o : vector.c

test_bdd.o : bdd.c
test_dictionary.o : dictionary.c
test_utils.o : utils.c
test_vector.o : vector.c
