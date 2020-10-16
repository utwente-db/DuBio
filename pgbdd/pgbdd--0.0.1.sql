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
