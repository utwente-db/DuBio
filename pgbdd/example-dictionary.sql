--
-- Example of the 'dictionary' type in the 'pgbdd; extension.
-- First do 'make install' in this directory to install the pgbdd extension.
-- This will create the SQL 'dictionary' type containing
-- variable/value/probabilitie pairs (vvp's) which are used to compute
-- probabilities of the Binary Decision Diagram (bdd) types.
-- The bdd's are implemented by the 'bdd' type, also in this extension.
--

--
-- First remove old tables and extensions and start fresh
--
drop table if exists Dict; -- must be first, before drop extension
drop extension if exists pgbdd;
create extension pgbdd;

-- create the main dictionary table
create table Dict (name varchar(20), dict dictionary);

-- create the 'mydict' dictionary
insert into Dict(name,dict) VALUES('mydict',dictionary('mydict'));

-- insert the first variable/value/prob pairs (vvp) in the 'mydict' dictionary
update Dict
set dict=add(dict,'x=0 : 0.6; x=1 : 0.4; ')
where name='mydict';

-- now look if it works
select print(dict) from Dict WHERE name = 'mydict';

-- add some more
update Dict
set dict=add(dict,'y=0:0.5;y=1:0.5; d=0:0.8;d=1:0.1;d=2:0.1;')
where name='mydict';
select print(dict) from Dict WHERE name = 'mydict';

-- delete the d=0 variable/value and look what happens to the probabilities
update Dict
set dict=del(dict,'d=0;')
where name='mydict';
select print(dict) from Dict WHERE name = 'mydict';

-- delete the complete d variable
update Dict
set dict=del(dict,'d=*;')
where name='mydict';
select print(dict) from Dict WHERE name = 'mydict';


-- update the y=0 prob to 1.0, look at the probabilities after the operation
update Dict
set dict=upd(dict,'y=0 : 1.0; ')
where name='mydict';
select print(dict) from Dict WHERE name = 'mydict';

-- and undo the previous update
update Dict
set dict=upd(dict,'y=0 : 0.5; y=1 : 0.5; ')
where name='mydict';
select print(dict) from Dict WHERE name = 'mydict';

-- and now delete the entire y variable again, now without '*'
update Dict
set dict=del(dict,'y=0 ; y=1 ; ')
where name='mydict';
select xxx(dict) from Dict WHERE name = 'mydict';
