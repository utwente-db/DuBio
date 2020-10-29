--
-- Example of the 'bdd' type in the 'pgbdd; extension.
-- First do 'make install' in this directory to install the pgbdd extension.
-- This will create the SQL 'bdd' (Binary Decision Diagram) type
--

--
-- First remove old tables and extensions and start fresh
--
drop extension if exists pgbdd cascade;
create extension pgbdd;

select bdd('x=1');

select tostring(bdd('x=1|y=1'));

select info(bdd('x=1|y=1'));

select dot(bdd('x=1|(x=2&y=4)'),'/tmp/x.dot');

CREATE OR REPLACE FUNCTION op(op text, lbdd bdd) RETURNS bdd
    AS $$ SELECT bdd(concat($1,'(',tostring($2),')')); $$
    LANGUAGE SQL;

CREATE OR REPLACE FUNCTION op(lbdd bdd, op text, rbdd bdd) RETURNS bdd
    AS $$ SELECT bdd(concat('(',tostring($1),')',$2,'(',tostring($3),')')); $$
    LANGUAGE SQL;

select op(bdd('x=1'),'&',bdd('y=2'));

select op('!',op(bdd('x=1'),'&',bdd('y=2')));

-- select dot(op('!',op(bdd('x=1'),'&',bdd('y=2'))),'/tmp/prettybrilliant.dot');

-- and finally some constant examples
select bdd('0');
select bdd('!0');
select op('!',op(bdd('0'),'&',bdd('1')));
select bdd('(x=1|x=2)&(!1)');
select prob(dictionary('empty'),bdd('0'));
select prob(dictionary('empty'),bdd('!(0&0)'));

select alg_bdd('base','(x=1&((y=1|y=2)&x=2))');
select alg_bdd('kaj' ,'(x=1&((y=1|y=2)&x=2))');
