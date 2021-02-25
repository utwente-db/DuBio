import time
from configparser import ConfigParser
from bddgen import BddGenerator
import psycopg2

VERBOSE            = True

SQL_START = """
--
DROP EXTENSION IF EXISTS pgbdd CASCADE;
CREATE EXTENSION pgbdd;
--
"""

def init_db():
    try:
       sql = SQL_START
       execute_pg(sql)
    except (Exception, psycopg2.DatabaseError) as error:
        print(error)
        exit() # pretty fatal
    
def config(configname='database.ini', section='postgresql'):
    # create a parser
    parser = ConfigParser()
    # read config file
    parser.read(configname)

    # get section, default to postgresql
    db = {}
    if parser.has_section(section):
        params = parser.items(section)
        for param in params:
            db[param[0]] = param[1]
    else:
        raise Exception('Section {0} not found in the {1} file'.format(section, configname))

    return db

conn_pg = None

def connect_pg(configname='database.ini'):
    """ Connect to the PostgreSQL database server """
    try:
        # read connection parameters
        params = config(configname=configname)

        # connect to the PostgreSQL server
        if VERBOSE:
            print('Connecting to the PostgreSQL database...')
        global conn_pg
        conn_pg = psycopg2.connect(**params)
		
        # create a cursor
        cur = conn_pg.cursor()
        
	# execute a statement
        if VERBOSE:
            print('PostgreSQL database version:')
        cur.execute('SELECT version()')

        # display the PostgreSQL database server version
        db_version = cur.fetchone()
        if VERBOSE:
            print(db_version)
    except (Exception, psycopg2.DatabaseError) as error:
        print(error)
        exit() # pretty fatal


def close_pg():
    """ Close connection to the PostgreSQL database server """
    try:
        if conn_pg is not None:
            conn_pg.close()
            if VERBOSE:
                print('Database connection closed.')
    except (Exception, psycopg2.DatabaseError) as error:
        print(error)

def execute_pg(sql_stat=None):
    """ Execute single command on the PostgreSQL database server """
    try:
        cur = conn_pg.cursor()
        cur.execute(sql_stat)
        cur.close()
    except (Exception, psycopg2.DatabaseError) as error:
        print(error)

def time_execute_pg(sql_stat=None):
    """ Execute single command on the PostgreSQL database server """
    try:
        cur = conn_pg.cursor()
        start = time.perf_counter()
        cur.execute(sql_stat)
        elapsed = time.perf_counter() - start
        cur.close()
        return elapsed * 1000 # ms
    except (Exception, psycopg2.DatabaseError) as error:
        print('# Postgres ERROR')
        print('# SQL  : '+sql_stat)
        print('# Error: '+str(error))
        exit()
#
#
#

def delete_dictionary():
  execute_pg("DROP TABLE IF EXISTS Dict;")

def create_dictionary():
  delete_dictionary();
  execute_pg("CREATE TABLE Dict (name varchar(20), dict dictionary);");
  brg = BddGenerator()
  cursor   = conn_pg.cursor()
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['base',brg.base_dict()])
  conn_pg.commit()

INSERTS_PER_COMMIT = 1000 

def generate_bdd_strings(n, table_name):
  brg = BddGenerator()
  execute_pg("DROP TABLE IF EXISTS {};".format(table_name))
  execute_pg("CREATE TABLE {} (rand_bdd varchar);".format(table_name));
  conn_pg.commit()
  inserted = 0
  cursor   = conn_pg.cursor()
  for i in range(0,n):
    cursor.execute("INSERT INTO {} (rand_bdd) VALUES(%s);".format(table_name), [brg.expression()])
    inserted = inserted + 1
    if inserted >= INSERTS_PER_COMMIT:
      inserted = 0
      conn_pg.commit()
      cursor.close()
      cursor   = conn_pg.cursor()
  conn_pg.commit()
  cursor.close()

SLEEP = 2.0

def clear_experiment(extension):
  bdd_str_tab = 'bdd_str_'+extension
  bdd_raw_tab = 'bdd_raw_'+extension
  execute_pg("DROP TABLE IF EXISTS {};".format(bdd_str_tab))
  execute_pg("DROP TABLE IF EXISTS {};".format(bdd_raw_tab))
  execute_pg("DROP TABLE IF EXISTS {};".format('tmp'))

def run_experiment(n,extension):
  res = []
  clear_experiment(extension)
  #
  bdd_str_tab     = 'bdd_str_'+extension
  bdd_raw_tab     = 'bdd_raw_'+extension
  #
  generate_bdd_strings(n,bdd_str_tab)
  #
  time.sleep(SLEEP)
  sql = 'SELECT rand_bdd INTO tmp FROM {};'.format(bdd_str_tab);
  t = time_execute_pg(sql)
  res.append(t)
  execute_pg("DROP TABLE IF EXISTS {};".format('tmp'))
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  time.sleep(SLEEP)
  sql = 'SELECT bdd(rand_bdd) INTO {} FROM {};'.format(bdd_raw_tab,bdd_str_tab);
  t = time_execute_pg(sql)
  res.append(t)
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  time.sleep(SLEEP)
  sql = 'select bdd, prob(dict,bdd)  FROM {}, Dict WHERE Dict.name=\'base\';'.format(bdd_raw_tab);
  t = time_execute_pg(sql)
  res.append(t)
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  time.sleep(SLEEP)
  sql = 'select max(prob(dict,bdd))  FROM {}, Dict WHERE Dict.name=\'base\';'.format(bdd_raw_tab);
  t = time_execute_pg(sql)
  res.append(t)
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  time.sleep(SLEEP)
  sql = 'SELECT concat(rand_bdd,\'(a=1|b=2)\') FROM {};'.format(bdd_str_tab);
  t = time_execute_pg(sql)
  res.append(t)
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  time.sleep(SLEEP)
  sql = 'SELECT bdd & bdd(\'(a=1|b=2)\') FROM {};'.format(bdd_raw_tab);
  t = time_execute_pg(sql)
  res.append(t)
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  time.sleep(SLEEP)
  sql = 'select max(prob(dict,bdd&bdd(\'(a=1|b=2)\')))  FROM {}, Dict WHERE Dict.name=\'base\';'.format(bdd_raw_tab);
  t = time_execute_pg(sql)
  res.append(t)
  print("Executed: "+sql)
  print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  clear_experiment(extension)
  conn_pg.commit()
  time.sleep(SLEEP)
  #
  return res

def simple_bdd_experiment():
  create_dictionary()
  #
  tab = []
  tab.append(run_experiment(1000,     '1K'))
  tab.append(run_experiment(5000,     '5K'))
  tab.append(run_experiment(10000,   '10K'))
  tab.append(run_experiment(50000,   '50K'))
  tab.append(run_experiment(100000, '100K'))
  tab.append(run_experiment(500000, '500K'))
  tab.append(run_experiment(1000000,  '1M'))
  tab.append(run_experiment(5000000,  '5M'))
  tab.append(run_experiment(10000000,'10M'))
  #
  # delete_dictionary()
  #
  for row in tab:
    for col in row:
      print("{:8.2f}  ".format(col),end='')
    print('')

def growing_dictionary_experiment():
  print("# Growing dictionary experiment.")
  delete_dictionary();
  execute_pg("CREATE TABLE Dict (name varchar(20), dict dictionary);");
  brg = BddGenerator()
  cursor   = conn_pg.cursor()
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['base',brg.base_dict()])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_1K',brg.extra_dict(4)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_10K',brg.extra_dict(80)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_40K',brg.extra_dict(400)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_80K',brg.extra_dict(800)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_160K',brg.extra_dict(1600)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_320K',brg.extra_dict(3200)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_480K',brg.extra_dict(4800)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_640K',brg.extra_dict(6400)])
  cursor.execute("INSERT INTO Dict (name,dict) VALUES(%s,%s);",['dict_960K',brg.extra_dict(9600)])
  #
  extension = '1K'
  bdd_str_tab = 'bdd_str_'+extension
  bdd_raw_tab = 'bdd_raw_'+extension
  generate_bdd_strings(1000,bdd_str_tab)
  sql = 'SELECT bdd(rand_bdd) INTO {} FROM {};'.format(bdd_raw_tab,bdd_str_tab);
  time_execute_pg(sql)
  res =  []
  dicts = ['dict_1K','dict_10K','dict_40K','dict_80K', 'dict_160K', 'dict_320K', 'dict_480K','dict_640K','dict_960K']
  for dsize in dicts:
    time.sleep(SLEEP)
    sql = 'select max(prob(dict,bdd))  FROM {}, Dict WHERE Dict.name=\'{}\';'.format(bdd_raw_tab,dsize);
    t = time_execute_pg(sql)
    res.append(t)
    print("Executed: "+sql)
    print("Time    : "+"{:.4f} ms.\n".format(t))
  #
  print(dicts)
  print(res)
  clear_experiment(extension)

if __name__ == '__main__':
    connect_pg(configname='database.ini')
    init_db()
    #
    simple_bdd_experiment()
    # growing_dictionary_experiment()
    #
    close_pg()
