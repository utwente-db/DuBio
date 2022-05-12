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
select bdd('x=1') &  bdd('y=1');
select bdd('x=1') -> bdd('y=1');
select bdd('(x=1|x=2)&(!1)');
select prob(dictionary(''),bdd('0'));
select prob(dictionary(''),bdd('!(0&0)'));

-- bdd property examples
select istrue(bdd('0'));
select isfalse(bdd('0'));
select hasvar(bdd('x=1&y=2'),'y');
select hasrva(bdd('x=1&y=2'),'x=1');

-- bdd contains / restrict examples
select contains(bdd('(x=1|x=2|x=3)'),      'x',2);
select contains(bdd('(x=1|x=2|x=3)'),      'x',9);
select contains(bdd('(x=1|x=2|x=3)'),      'x',-1);

select restrict(bdd('(x=1|x=2|x=3)'),      'x', 2,FALSE);
select restrict(bdd('(x=1|x=2|x=3)'),      'x',-1,FALSE);
select restrict(bdd('(x=1|x=2|x=3)&(y=1)'),'x', 2,TRUE);
select restrict(bdd('(x=1|x=2|x=3)&(y=1)'),'x', 2,FALSE);

-- nice examples
select( bdd('(x=1)') & bdd('(y=1|y=2)&x=2') );
select( bdd('z=1') & ! (bdd('(x=1)') & bdd('(y=1|y=2)&x=2')) );
-- select alg_bdd('robdd','(z=1)&!((x=1)&((y=1|y=2)&x=2))' ); -- wrong !!!
select alg_bdd('base','(z=1)&!((x=1)&((y=1|y=2)&x=2))' );
select bdd( '(z=1)&!((x=1)&((y=1|y=2)&x=2))' );



-- equal en equiv examples
select bdd_equal(bdd('0'),bdd('0'));
select bdd_equal(bdd('0'),bdd('1'));
select bdd_equal(bdd('x=1'),bdd('x=1'));
select bdd_equal(bdd('x=1&x=1'),bdd('x=1'));
select bdd_equal(bdd('x=1'),bdd('y=1'));
select bdd_equiv(bdd('x=1&x=2'),bdd('0'));
select bdd_equiv(bdd('x=1'),bdd('y=1'));
select bdd_equiv(bdd('(!(a=1|c=0)|!(b=2))'),bdd('((a=1&!b=2)|(!a=1&(!b=2|!c=0)))'));

-- examples of boolean ops one uses apply and second one text concat and
-- reparse, because of Kaj algorithm the second one detects the conflict
-- in the Bdd. Both expressions are equivalent but not equal when apply is 
-- used
select _op_bdd('&',bdd('x=1'),bdd('x=2')); -- uses apply
select _op_bdd_by_text('&',bdd('x=1'),bdd('x=2'));

select bdd_equal(bdd('x=1')&bdd('x=2'),bdd('0'));
select bdd_equiv(bdd('x=1')&bdd('x=2'),bdd('0'));
select bdd_equal(_op_bdd_by_text('&',bdd('x=1'),bdd('x=2')),bdd('0'));
