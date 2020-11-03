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

--
select bdd('x=1');
select tostring(bdd('x=1|y=1'));
select info(bdd('x=1|y=1'));
select dot(bdd('x=1|(x=2&y=4)'),'/tmp/x.dot');

-- constant examples
select bdd('0');
select bdd('!0');
select ! bdd('!0');
select bdd('0') & bdd('1');
select !(bdd('0') | bdd('1'));
select bdd('0') | bdd('1') | bdd('0') | bdd('1');

-- a couple of more difficult examples
select bdd('(x=1|x=2)&(!1)');
select prob(dictionary(''),bdd('0'));
select prob(dictionary(''),bdd('!(0&0)'));

-- different algorithms
select alg_bdd('base',' (x=1&((y=1|y=2)&x=2))');
select alg_bdd('robdd','(x=1&((y=1|y=2)&x=2))');
select alg_bdd('kaj',  '(x=1&((y=1|y=2)&x=2))');

-- bdd property examples
select istrue(bdd('0'));
select isfalse(bdd('0'));
select hasvar(bdd('x=1&y=2'),'y');
select hasrva(bdd('x=1&y=2'),'x=1');

-- nice examples
select( bdd('(x=1)') & bdd('(y=1|y=2)&x=2') );
select( bdd('z=1') & ! (bdd('(x=1)') & bdd('(y=1|y=2)&x=2')) );
select alg_bdd('robdd','(z=1)&!((x=1)&((y=1|y=2)&x=2))' ); -- wrong !!!
select alg_bdd('base','(z=1)&!((x=1)&((y=1|y=2)&x=2))' );
select bdd( '(z=1)&!((x=1)&((y=1|y=2)&x=2))' );

