/* ----------
 * pgbdd_interface.sql
 *
 *      Copyright (c) 2020 Jan Flokstra
 *      Author:  Jan Flokstra
 *	License: BSD
 *
 * ----------
 */


/*------------------------
 * Definition of BDD type.
 *------------------------
 */ 

create 
function bdd_in(expression cstring) returns bdd
     as '$libdir/pgbdd', 'bdd_in'
     language C immutable strict;
comment on function bdd_in(cstring) is
'Create a bdd expression from argument string.';

create 
function bdd_out(dict bdd) returns cstring
     as '$libdir/pgbdd', 'bdd_out'
     language C immutable strict;
comment on function bdd_out(bdd) is
'create a serialised TEXT representation of a bdd.';

CREATE TYPE bdd (
    input = bdd_in,
    output = bdd_out,
    internallength = variable,
    alignment = double,
    storage = main
);
comment on type bdd is
'A postgres implementation of a Binary Decision Diagrams.';

create 
function _op_bdd(operator cstring,lhs_bdd bdd,rhs_bdd bdd) returns bdd
     as '$libdir/pgbdd', 'bdd_pg_operator'
     language C immutable; -- not STRICT because rhs_bdd may be NULL

create 
function alg_bdd(alg cstring, expression cstring) returns bdd
     as '$libdir/pgbdd', 'alg_bdd'
     language C immutable strict;
comment on function alg_bdd(cstring,cstring) is
'Create a bdd expression from argument algorithm and string.';

create 
function tostring(bdd bdd) returns text
     as '$libdir/pgbdd', 'bdd_pg_tostring'
     language C immutable strict;
comment on function tostring(bdd) is
'get TEXT representation of expression of bdd.';

create 
function info(bdd bdd) returns text
     as '$libdir/pgbdd', 'bdd_pg_info'
     language C immutable strict;

comment on function info(bdd) is
'get expr/tree/dot info of dictionary.';

create 
function dot(bdd bdd, file cstring default '') returns text
     as '$libdir/pgbdd', 'bdd_pg_dot'
     language C immutable strict;
comment on function dot(bdd,cstring) is
'create a Graphviz DOT string representation of dictionary. The second argument is optional filename to store dot file';

create 
function _bdd_has_property(bdd bdd, prop integer, str_prop cstring) returns BOOLEAN
     as '$libdir/pgbdd', 'bdd_has_property'
     language C immutable strict;

CREATE OR REPLACE FUNCTION istrue(bdd bdd) RETURNS BOOLEAN
    AS $$ SELECT _bdd_has_property($1,1,''); $$
    LANGUAGE SQL STRICT;
comment on function istrue(bdd) is
'Returns TRUE if bdd() is just 1.';

CREATE OR REPLACE FUNCTION isfalse(bdd bdd) RETURNS BOOLEAN
    AS $$ SELECT _bdd_has_property($1,0,''); $$
    LANGUAGE SQL STRICT;
comment on function istrue(bdd) is
'Returns TRUE if bdd() is just 0.';

CREATE OR REPLACE FUNCTION hasvar(bdd bdd,v text) RETURNS BOOLEAN
    AS $$ SELECT _bdd_has_property($1,2,cstring($2)); $$
    LANGUAGE SQL STRICT;
comment on function hasvar(bdd,text) is
'Returns TRUE var v is used in bdd.';

CREATE OR REPLACE FUNCTION hasrva(bdd bdd,rva text) RETURNS BOOLEAN
    AS $$ SELECT _bdd_has_property($1,3,cstring($2)); $$
    LANGUAGE SQL STRICT;
comment on function hasvar(bdd,text) is
'Returns TRUE rva (v=n) is used in bdd.';

--
-- The operator and syntactic sugar section for bdd's
--

CREATE OR REPLACE FUNCTION _not(lbdd bdd) RETURNS bdd
    AS $$ SELECT _op_bdd('!',$1,NULL); $$
    LANGUAGE SQL IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION _or(lbdd bdd,rbdd bdd) RETURNS bdd
    AS $$ SELECT _op_bdd('|',$1,$2); $$
    LANGUAGE SQL IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION _and(lbdd bdd,rbdd bdd) RETURNS bdd
    AS $$ SELECT _op_bdd('&',$1,$2); $$
    LANGUAGE SQL IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION _implies(lbdd bdd,rbdd bdd) RETURNS bdd
    AS $$ SELECT (_or(_not(lbdd),rbdd)); $$
    LANGUAGE SQL IMMUTABLE STRICT;

create operator !  (procedure  = _not                    , rightarg = bdd); 
create operator &  (procedure  = _and     , leftarg = bdd, rightarg = bdd, commutator = &); 
create operator |  (procedure  = _or      , leftarg = bdd, rightarg = bdd); 
create operator -> (procedure  = _implies , leftarg = bdd, rightarg = bdd); 

/*------------------------------
 * Definition of DICTIONARY type.
 *-------------------------------
 */ 

create 
function dictionary_in(dictname cstring) returns dictionary
     as '$libdir/pgbdd', 'dictionary_in'
     language C immutable strict;
comment on function dictionary_in(cstring) is
'Create a dictionary with name dictname and the vardef variable definitions.';

create 
function dictionary_out(dict dictionary) returns cstring
     as '$libdir/pgbdd', 'dictionary_out'
     language C immutable strict;
comment on function dictionary_out(dictionary) is
'create a serialised string representation of dictionary.';

CREATE TYPE dictionary (
    input = dictionary_in,
    output = dictionary_out,
    internallength = variable,
    alignment = double,
    storage = main
);
comment on type dictionary is
'A postgres implementation of a variable/probability dictionary used by Binary Decision Diagrams.';

create
function _dict_modify(dict dictionary, mode integer, vardef cstring) returns dictionary
     as '$libdir/pgbdd', 'dictionary_modify'
     language C immutable strict;

CREATE OR REPLACE FUNCTION add(d dictionary, vardefs text) RETURNS dictionary
    AS $$ SELECT _dict_modify($1,1,cstring($2)); $$
    LANGUAGE SQL;
comment on function add(dictionary, text) is
'Add the var=val:prob variable definitions to the dictionary.';

CREATE OR REPLACE FUNCTION del(d dictionary, vardefs text) RETURNS dictionary
    AS $$ SELECT _dict_modify($1,2,cstring($2)); $$
    LANGUAGE SQL;
comment on function del(dictionary, text) is
'Delete the var=val variable definitions from the dictionary.';

CREATE OR REPLACE FUNCTION upd(d dictionary, vardefs text) RETURNS dictionary
    AS $$ SELECT _dict_modify($1,3,cstring($2)); $$
    LANGUAGE SQL;
comment on function upd(dictionary, text) is
'Update the var=val:prob variable definitions in the dictionary.';

create 
function merge(ldict dictionary, rdict dictionary) returns dictionary
     as '$libdir/pgbdd', 'dictionary_merge'
     language C immutable strict;
comment on function merge(dictionary, dictionary) is
'Merge 2 dictionary into a new.';

create 
function print(dict dictionary) returns text
     as '$libdir/pgbdd', 'dictionary_print'
     language C immutable strict;
comment on function print(dictionary) is
'create a serialised string representation of dictionary.';

create 
function debug(dict dictionary) returns text
     as '$libdir/pgbdd', 'dictionary_debug'
     language C immutable strict;
comment on function debug(dictionary) is
'create a serialised string representation of internal dictionary structure.';

--
-- The operator and syntactic sugar section for bdd's
--

create operator + (procedure=merge,leftarg=dictionary,rightarg=dictionary); 

CREATE AGGREGATE sum (dictionary)
(   
    sfunc = merge,
    stype = dictionary
);

create or replace function and_accum(internal bdd, next bdd) returns bdd
 AS $$
select case when internal is NULL then next
                                  else _and($1,$2)
 end;
$$
 language SQL;

create or replace aggregate agg_and (bdd)
(
 SFUNC = and_accum,
 STYPE = bdd
);

create or replace function or_accum(internal bdd, next bdd) returns bdd
 AS $$
select case when internal is NULL then next
                                  else _or($1,$2)
 end;
$$
 language SQL;

create or replace aggregate agg_or (bdd)
(
 SFUNC = or_accum,
 STYPE = bdd
);

/*
 *
 * Mixed DICTIONARY/BDD functions/operations
 */

create 
-- function prob(dict dictionary, bdd bdd) returns numeric CRASHES SERVER
function prob(dict dictionary, bdd bdd) returns double precision
     as '$libdir/pgbdd', 'bdd_pg_prob'
     language C immutable strict;
comment on function prob(dictionary, bdd) is
'return probability of bdd expression using rva/probabilities defined in dictionary.';
