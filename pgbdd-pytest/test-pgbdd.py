import sys
import json
import gzip
from configparser import ConfigParser
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
    
def examplexxx(tablebase, cursor, offer):
    try:
        all_args = [offer['url'], offer['nodeID'], int(offer['cluster_id'])]
        p_names  = ''
        p_perc   = ''
        #
        prop = get_top_properties(offer)
        for k in prop.keys():
            p_names += ',p_'+k
            p_perc  += ',%s'
            all_args.append(prop[k])
            # print(k,prop[k])
        cursor.execute('INSERT INTO {}_offer(key,cluster_id{}) VALUES(insert_wdc_key(%s,%s),%s{});'.format(tablebase,p_names,p_perc),all_args)
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

if __name__ == '__main__':
    connect_pg(configname='database.ini')
    init_db()
    # analyze_json(jsonzip='./sample_offersenglish.json.gz')
    # convert_json(jsonzip='./offers_english.json.gz',tablebase='WDC_ENG')
    # convert_json(jsonzip='./sample_offersenglish.json.gz',tablebase='WDC_ENG')
    close_pg()
