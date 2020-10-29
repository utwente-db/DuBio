/* ----------
 * pgbdd_interface.sql
 *
 *      Copyright (c) 2020 Jan Flokstra
 *      Author:  Jan Flokstra
 *	License: BSD
 *
 * ----------
 */

create 
function bdd_in(expression cstring) returns bdd
     as '$libdir/pgbdd', 'bdd_in'
     language C immutable strict;

comment on function bdd_in(cstring) is
'Create a bdd expression from argument string.';

create 
function alg_bdd(alg cstring, expression cstring) returns bdd
     as '$libdir/pgbdd', 'alg_bdd'
     language C immutable strict;

comment on function alg_bdd(cstring,cstring) is
'Create a bdd expression from argument algorithm and string.';

create 
function bdd_out(dict bdd) returns cstring
     as '$libdir/pgbdd', 'bdd_out'
     language C immutable strict;

comment on function bdd_out(bdd) is
'create a serialised string representation of a bdd.';


create type bdd (
    input = bdd_in,
    output = bdd_out,
    internallength = variable,
    alignment = double,
    storage = main
);

comment on type bdd is
'A postgres implementation of a Binary Decision Diagrams.';


create 
function tostring(bdd bdd) returns cstring
     as '$libdir/pgbdd', 'bdd_pg_tostring'
     language C immutable strict;

comment on function tostring(bdd) is
'get string representation of expression of bdd.';

create 
function info(bdd bdd) returns cstring
     as '$libdir/pgbdd', 'bdd_pg_info'
     language C immutable strict;

comment on function info(bdd) is
'get expr/tree/dot info of dictionary.';

create 
function dot(bdd bdd, file cstring default '') returns cstring
     as '$libdir/pgbdd', 'bdd_pg_dot'
     language C immutable strict;

comment on function dot(bdd,cstring) is
'create a Graphviz DOT string representation of dictionary. The second argument is optional filename to store dot file';

/*
 *
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


create type dictionary (
    input = dictionary_in,
    output = dictionary_out,
    internallength = variable,
    alignment = double,
    storage = main
);

comment on type dictionary is
'A postgres implementation of a variable/probability dictionary used by Binary Decision Diagrams.';

create 
function add(dict dictionary, vardef cstring) returns dictionary
     as '$libdir/pgbdd', 'dictionary_add'
     language C immutable strict;

comment on function add(dictionary, cstring) is
'Add the var=val:prob variable definitions to the dictionary.';

create 
function del(dict dictionary, vardef cstring) returns dictionary
     as '$libdir/pgbdd', 'dictionary_del'
     language C immutable strict;

comment on function del(dictionary, cstring) is
'Delete the var=val variable definitions from the dictionary.';

create 
function upd(dict dictionary, vardef cstring) returns dictionary
     as '$libdir/pgbdd', 'dictionary_upd'
     language C immutable strict;

comment on function upd(dictionary, cstring) is
'Update the var=val:prob variable definitions in the dictionary.';


create 
function print(dict dictionary) returns cstring
     as '$libdir/pgbdd', 'dictionary_print'
     language C immutable strict;

comment on function print(dictionary) is
'create a serialised string representation of dictionary.';


create 
function prob(dict dictionary, bdd bdd) returns double precision
     as '$libdir/pgbdd', 'bdd_pg_prob'
     language C immutable strict;

comment on function prob(dictionary, bdd) is
'return probability of bdd expression with rva/probabilities defined in dictionary.';
