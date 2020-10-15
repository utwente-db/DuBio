drop table if exists Dict;
drop extension pgbdd;
create extension pgbdd;

create table Dict (name varchar(20), dict dictionary);

insert into Dict(name,dict) VALUES('mydict',dictionary('mydict'));

select dict from Dict WHERE name = 'mydict';

update Dict
set dict=add(dict,'x=0 : 0.6; x=1 : 0.4; ')
where name='mydict';

select print(dict) from Dict WHERE name = 'mydict';

update Dict
set dict=add(dict,'y=0 : 0.5; y=1 : 0.5; ')
where name='mydict';

select print(dict) from Dict WHERE name = 'mydict';

update Dict
set dict=add(dict,'y=2 : 0.5; y=3 : 0.5; ')
where name='mydict';

select print(dict) from Dict WHERE name = 'mydict';

update Dict
set dict=add(dict,'x=2 : 0.4; ')
where name='mydict';

select print(dict) from Dict WHERE name = 'mydict';
