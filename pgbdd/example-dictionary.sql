--
-- Example of the pgbdd dictionary type
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

-- add a few more
update Dict
set dict=add(dict,'y=0 : 0.5; y=1 : 0.5; ')
where name='mydict';

-- and look again
select print(dict) from Dict WHERE name = 'mydict';

-- add a few more 'y' vvp's
update Dict
set dict=add(dict,'y=2 : 0.5; y=3 : 0.5; ')
where name='mydict';

-- and look what happened to the probabilities
select print(dict) from Dict WHERE name = 'mydict';

update Dict
set dict=add(dict,'x=2 : 0.4; ')
where name='mydict';

select print(dict) from Dict WHERE name = 'mydict';
