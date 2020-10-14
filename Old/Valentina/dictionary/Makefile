EXTENSION = dubio       # the extensions name
DATA = dubio--0.0.1.sql  # script files to install
#ßREGRESS = myext_test     # our test script file (without extension)
MODULES = dictionary       # our c module file to build

# postgres build stuff
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
