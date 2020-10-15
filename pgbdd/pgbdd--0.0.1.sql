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
function dictionary_add(dict dictionary, vardef cstring) returns dictionary
     as '$libdir/pgbdd', 'dictionary_add'
     language C immutable strict;

comment on function dictionary_add(dictionary, cstring) is
'Add the vardef variable defintions to the dictionary.';


create 
function dictionary_print(dict dictionary) returns cstring
     as '$libdir/pgbdd', 'dictionary_print'
     language C immutable strict;

comment on function dictionary_print(dictionary) is
'create a serialised string representation of dictionary.';
