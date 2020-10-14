/* ----------
 * mystr_interface.sql
 *
 *      Copyright (c) 2020 Jan Flokstra
 *      Author:  Jan Flokstra
 *	License: BSD
 *
 * ----------
 */

create 
function mystr_in(textin cstring) returns mystr
     as '$libdir/mystr', 'mystr_in'
     language C immutable strict;

comment on function mystr_in(cstring) is
'Read the serialised string representation of a mystr, TEXTIN, into a mystr.';


create 
function mystr_out(mystr mystr) returns cstring
     as '$libdir/mystr', 'mystr_out'
     language C immutable strict;

comment on function mystr_out(mystr) is
'create a serialised string representation of mystr.';


create type mystr (
    input = mystr_in,
    output = mystr_out,
    internallength = variable,
    alignment = double,
    storage = main
);

comment on type mystr is
'A postgres implementation of Binary Decision Diagrams.';

create 
function mystr_add(mystr1 mystr, mystr2 mystr) returns mystr
     as '$libdir/mystr', 'mystr_add'
     language C immutable strict;

comment on function mystr_add(mystr, mystr) is
'Return the concatenation of two mystr values.';

create operator + (
    procedure = mystr_add,
    leftarg = mystr,
    rightarg = mystr
);

create 
function mystr_equal(mystr1 mystr, mystr2 mystr) returns bool
     as '$libdir/mystr', 'mystr_equal'
     language C immutable strict;

comment on function mystr_equal(mystr, mystr) is
'Predicate returning true if mystr1 and mystr2 represent the same condition.';

create operator = (
    procedure = mystr_equal,
    leftarg = mystr,
    rightarg = mystr,
    commutator = =,
    negator = <>
);

create 
function mystr_nequal(mystr1 mystr, mystr2 mystr) returns bool
     as '$libdir/mystr', 'mystr_nequal'
     language C immutable strict;

comment on function mystr_nequal(mystr, mystr) is
'Predicate returning true if mystr1 and mystr2 do not represent the same condition.';

create operator <> (
    procedure = mystr_nequal,
    leftarg = mystr,
    rightarg = mystr,
    commutator = <>,
    negator = =
);

create operator class mystr_ops
    default for type mystr  using btree as
        operator        1       = ,
        operator        2       <>;
