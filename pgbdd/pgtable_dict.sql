DROP TABLE IF EXISTS mydata;
DROP TABLE IF EXISTS rva_dictionary;

CREATE TABLE mydata(
   id SERIAL,
   myname VARCHAR(50),
   bddvars TEXT
);

CREATE TABLE rva_dictionary(
   rva  VARCHAR(16),
   prob DOUBLE PRECISION
);
CREATE INDEX ON rva_dictionary (rva);

INSERT into rva_dictionary (rva,prob) VALUES('a=1',0.5);
INSERT into rva_dictionary (rva,prob) VALUES('a=2',0.5);
INSERT into rva_dictionary (rva,prob) VALUES('b=1',0.333);
INSERT into rva_dictionary (rva,prob) VALUES('b=2',0.333);
INSERT into rva_dictionary (rva,prob) VALUES('b=3',0.333);

INSERT into mydata (myname,bddvars) VALUES('Ram',  'a=1');
INSERT into mydata (myname,bddvars) VALUES('Sham', 'a=1,b=1');
INSERT into mydata (myname,bddvars) VALUES('Bham', 'b=1');
INSERT into mydata (myname,bddvars) VALUES('Kam',  'a=2,b=2');
INSERT into mydata (myname,bddvars) VALUES('Lam',  'a=1,b=1,b=2');
INSERT into mydata (myname)      VALUES('Lam');

SELECT * FROM MYDATA;

SELECT I.id,I.myname,I.bddvars,string_agg(J.rva||':'||J.prob, ';') AS rva_prob
FROM mydata I 
LEFT JOIN rva_dictionary J 
ON J.rva = ANY(STRING_TO_ARRAY(I.bddvars,',')) 
GROUP BY myname,I.id, I.bddvars ORDER BY id
