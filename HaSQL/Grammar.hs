module Grammar where
{-
Author: Jochem Groot Roessink
Contains datatypes that are used to generate a parse tree
-}

-- Any command
data Command
    = Select [Col] [Refine] -- SELECT command
    | Other [String] -- An unknown command, is not converted and executed as normal SQL
    deriving (Show, Eq)
-- When adding to the Command datatype be sure to update
-- isQuery in GrammarFunctions.hs as well

-- Column of a table
data Col
    = AllOf Name -- All elements or all elements of a table: *, table.*
    | Col Value -- Column is only a value
    | ColAs Value String -- Column has a value and a name: SELECT id AS identifier
    deriving (Show, Eq)

-- Table container
-- Contains the name of a table and zero or more JOIN statements
-- SELECT * FROM table1 [JOIN table2 ON table1.id = table2.id]
data TabCont
    = Tabs Table [(Join, Table, Condition)]
    deriving (Show, Eq)

-- Table
data Table
    = Table Name -- Only the name of the table
    | TableAs Name String -- Table name and new name: SELECT * FROM table1 as t
    deriving (Show, Eq)

-- JOIN statements, used in TabCont
data Join
    = Inner -- (INNER) JOIN
    | Lft -- LEFT (OUTER) JOIN
    | Rght -- RIGHT (OUTER) JOIN
    | Full -- FULL (OUTER) JOIN
    deriving (Show, Eq)

-- Refines a command, like SELECT
data Refine
    = From [TabCont] -- FROM table1 [, table2, etc.]
    | Where Condition -- WHERE id > 3
    | GroupBy Name -- GROUP BY column
    | OrderBy Expr Order -- ORDER BY column [ASC|DESC]
    | Limit String -- LIMIT 1
    | Into Name -- INTO table
    deriving (Show, Eq)
-- When adding a new refine element also update
-- the refMap function with the right order

-- Order
data Order
    = Asc -- Ascending
    | Desc -- Descending
    deriving (Show, Eq)

-- Name, list of strings seperated by dots
data Name
    = Name [String] -- "table.column" --> ["table", "column"]
    deriving (Show, Eq)

-- Condition, used in WHERE statement
data Condition
    = Compare Value Operator Value -- Compare two values, id > 3, name = 'jochem'
    | Between Expr Expr Expr -- age BETWEEN 18 AND 30
    | And Condition [Condition] -- age > 18 AND age < 30 AND name = 'jochem'
    | Or Condition [Condition] -- age <= 18 OR age >= 30
    | Not Condition -- NOT (age > 65)
    | IsTrue -- TRUE
    | IsFalse -- FALSE
    deriving (Show, Eq)

-- Value
data Value
    = Type Value String -- Value with a typecast: 3::numeric
    | Exp Expr -- Expression: 30, column, column+30, function(column, 30)
    | Tuple [Value] -- (val1, val2, val3)
    | Cond Condition -- Condition: TRUE, age > 18
    | Text String -- 'some text'
    | Case [(Condition, Value)] (Maybe Value) -- CASE WHEN age > 18 THEN val1 [WHEN age < 18 THEN val2] [ELSE val3]
    | Comm Command -- nested command, can only be a select command
    deriving (Show, Eq)

-- Expression
data Expr
    = Calc Expr [(Operator, Expr)] -- 3+6-2, age*12
    | ColCall Name -- column
    | FunCall String [Value] -- func(column, 69)
    | Num String -- 3, 3.6, -4
    deriving (Show, Eq)

-- Operator
data Operator
    -- comparison
    = Equal
    | Greater
    | Less
    | GreatEq
    | LessEq
    | NotEq
    | Like
    | In
    -- arithmetic
    | Add
    | Subtract
    | Modulo
    | Multiply
    | Divide
    | Power
    -- bitwise
    | BAnd
    | BOr
    -- condition
    | CAnd
    | COr
    | CNot
    -- zero
    | Zero -- used in Printer.hs, to notify that parantheses are not necessary
    deriving (Show, Eq)