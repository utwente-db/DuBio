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
drop extension if exists pgbdd cascade;
create extension pgbdd;

-- create the main dictionary table
create table Dict (name varchar(20), dict dictionary);

-- create the 'mydict' dictionary
insert into Dict(name,dict) VALUES('mydict',dictionary(''));

-- insert the first variable/value/prob pairs (vvp) in the 'mydict' dictionary
update Dict
set dict=add(dict,'x=1:0.4; x=2:0.6 ; y=1:0.2; y=2:0.8; ')
where name='mydict';

-- now compute first simple prob!
-- select prob(dict,bdd('(x=1|y=1)')) from Dict WHERE name = 'mydict';

drop table if exists T;
create table T (name varchar(20), bdd bdd);
insert into T VALUES('tuple1',bdd('(x=1|x=2)'));
insert into T VALUES('tuple2',bdd('(x=1|y=1)'));
insert into T VALUES('tuple2',bdd('(x=1&y=1)'));
select T.name,tostring(T.bdd),prob(dict,T.bdd) from T,Dict WHERE Dict.name='mydict';
--
-- explain select T.name,tostring(T.bdd),prob(ref(dict),T.bdd) from T,Dict WHERE Dict.name='mydict';
--
-- select T.name,tostring(T.bdd),prob(refdict.d,T.bdd) FROM (select ref(dict) AS d from Dict WHERE Dict.name='mydict') AS refdict,T;
--
-- select prob(refdict.d,T.bdd) FROM T CROSS JOIN LATERAL (select ref(dict) AS d from Dict WHERE Dict.name='mydict') AS refdict;
--
-- WITH refdict AS (select ref(dict) AS d from Dict WHERE Dict.name='mydict') 
--     SELECT T.name,tostring(T.bdd),prob(refdict.d,T.bdd) from T,refdict;
--
CREATE OR REPLACE FUNCTION singular_dict_ref(in dict_name varchar)
RETURNS dictionary_ref AS $$
   SELECT ref(Dict.dict) FROM Dict WHERE Dict.name = dict_name
$$ LANGUAGE sql IMMUTABLE;

SELECT T.name,tostring(T.bdd),prob(singular_dict_ref('mydict'),T.bdd) FROM T;
