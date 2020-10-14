-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION dubio" to load this file. \quit
CREATE FUNCTION create_dictionary() RETURNS bytea
AS '$libdir/dictionary'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION add_variable(text,bytea) RETURNS bytea
AS '$libdir/dictionary'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION remove_var(text,bytea) RETURNS bytea
AS '$libdir/dictionary'
LANGUAGE C IMMUTABLE STRICT;


CREATE OR REPLACE FUNCTION create_dict(id integer)
RETURNS void AS
$BODY$
DECLARE
b   bytea;
begin
  CREATE TABLE IF NOT EXISTS dictionaryTable (id integer UNIQUE NOT NULL, dictionary bytea NOT NULL);
  b := create_dictionary();
  INSERT INTO dictionaryTable VALUES(id, b);
end;
$BODY$
LANGUAGE plpgsql VOLATILE;

CREATE OR REPLACE FUNCTION add_var(expr text, i integer)
RETURNS void AS
$BODY$
DECLARE
b   bytea; res bytea;
begin
  b := (SELECT dictionaryTable.dictionary FROM dictionaryTable WHERE id = i );
  if b is NULL THEN
    RAISE NOTICE 'There is not dictionary with id = %', i;
    return;
  end if;
  res := add_variable(expr, b);
  UPDATE dictionaryTable SET dictionary = res WHERE id = i;
end;
$BODY$
LANGUAGE plpgsql VOLATILE;

CREATE OR REPLACE FUNCTION remove_variable(name text, i integer)
RETURNS void AS
$BODY$
DECLARE
b   bytea; res bytea;
begin
  b := (SELECT dictionaryTable.dictionary FROM dictionaryTable WHERE id = i) ;
  if b is NULL THEN
    RAISE NOTICE 'There is not dictionary with id = %', i;
    return;
  end if;
  res := remove_var(name, b);
  UPDATE dictionaryTable SET dictionary = res WHERE id = i;
end;
$BODY$
LANGUAGE plpgsql VOLATILE;
