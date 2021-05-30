module Converter where
{-
Converts that parse tree to a parse tree that
after it is printed can be executed by DuBio
Author: Jochem Groot Roessink
-}

import Grammar
import GrammarFunctions
import Names
import Data.List

{-
Conversions that happen before the database is asked
which tables are probabilistic
-}


preConv :: [Command] -> [Command]
preConv [Other []] = [] -- delete empty command
preConv [cmd] = [cmd]
preConv (cmd:xs) = preConv [cmd] ++ preConv xs

{-
Main conversions
-}

-- Converts the entire program, boolean list required for every command
-- which says which tables are probabilistic
convProgram :: [Command] -> [[Bool]] -> [Command]
convProgram cmds bools = zipWith convCommand cmds $ zipWith getProb cmds bools

-- Converts a command, 2nd arg is the probability
convCommand :: Command -> Expr -> Command
convCommand (Select cols refs) prob = Select (convCols newCols prob) (convRefs refs prob) where
        newCols = convAll cols names
        names = map tableToDisplay (getRefTables refs)
convCommand (Other strs) _ = Other strs -- not converted

-- Converts * so that _dict is not included
-- select * from person -> select person.* from person, _dict
convAll :: [Col] -> [Name] -> [Col]
convAll [] _ = []
convAll (AllOf (Name ["*"]):xs) names = newAll ++ xs where
    newAll = map (\n -> AllOf n) cols
    cols = map (\(Name xs) -> Name (xs++["*"])) names
convAll (col:xs) names = (col:convAll xs names)

-- Converts the Column list
convCols :: [Col] -> Expr -> [Col]
convCols cols prob = map (flip convCol prob) cols

-- Converts a Column
convCol :: Col -> Expr -> Col
convCol (ColAs val name) prob = ColAs (convValue val prob) name
convCol (Col (Exp (ColCall (Name [name])))) prob | name == probAttr = ColAs (Exp prob) probName -- display _prob
convCol (Col val) prob = Col (convValue val prob)
convCol col _ = col

-- Converts a Value
convValue :: Value -> Expr -> Value
convValue (Exp exp) prob = Exp $ convExpr exp prob
convValue (Cond cond) prob = Cond $ convCond cond prob
convValue (Tuple vals) prob = Tuple (map (flip convValue prob) vals)
convValue (Type val typ) prob = Type (convValue val prob) typ
convValue val _ = val

-- Converts an expression
convExpr :: Expr -> Expr -> Expr
convExpr (ColCall (Name [name])) prob | name == probAttr = prob -- change _prob by probability function
convExpr (Calc exp opExps) prob = Calc (convExpr exp prob) (map func opExps) where
    func = (\(a,b) -> (a, convExpr b prob))
convExpr exp _ = exp

-- Converts a condition
convCond :: Condition -> Expr -> Condition
convCond (And cond conds) prob = And (convCond cond prob) (map (flip convCond prob) conds)
convCond (Or cond conds) prob = Or (convCond cond prob) (map (flip convCond prob) conds)
convCond (Not cond) prob = Not (convCond cond prob)
convCond (Compare val1 op val2) prob = Compare (convValue val1 prob) op (convValue val2 prob)
convCond (Between ex1 ex2 ex3) prob = Between (c ex1) (c ex2) (c ex3) where
    c ex = convExpr ex prob
convCond cond _ = cond

-- Converts the Refine list
convRefs :: [Refine] -> Expr -> [Refine]
convRefs refs prob
    | containsWhere refs = map (flip convRef prob) refs
    | otherwise = sortBy refSorter (Where dictCond:map (flip convRef prob) refs) -- Add WHERE _dict.name = 'mydict'

-- Converts a Refine datatype
convRef :: Refine -> Expr -> Refine
convRef (From tabConts) prob = From (convTabConts tabConts prob)
convRef (Where cond) prob = Where $ convCond (convWhereCond cond) prob -- Add _dict.name = 'mydict' to WHERE
convRef (OrderBy exp ord) prob = OrderBy (convExpr exp prob) ord
convRef ref _ = ref

-- Converts a Table Container
convTabConts :: [TabCont] -> Expr -> [TabCont]
convTabConts conts prob = newConts ++ [(Tabs (Table (Name [dictTableName])) [])] where
    newConts = map func conts -- convert conditions in the table container
    func = (\(Tabs t jtc) -> Tabs t (map func2 jtc))
    func2 = (\(j,t,c) -> (j,t,convCond c prob))

-- Adds the _dict.name = 'mydict' to WHERE
convWhereCond :: Condition -> Condition
convWhereCond (And cond xs) = And cond (xs ++ [dictCond])
convWhereCond cond = And cond [dictCond]

{-
Conversion that happens after the SQL is generated
Only used for representing the results is Haskell
so not needed in final product
-}

-- Add titles to unnamed columns in every command
postConv :: Command -> Command
postConv (Select cols refs) = Select newCols refs where
    newCols = [postConvCol col i | i <- [0..length cols-1], let col = cols!!i]
postConv cmd = cmd

-- Add titles to unnamed columns in a command
postConvCol :: Col -> Int -> Col
postConvCol (AllOf name) i = AllOf name
postConvCol (Col (Exp (FunCall name vals))) i = ColAs (Exp $ FunCall name vals) (name ++ " (" ++ show (i+1) ++ ")")
postConvCol (Col (Exp (ColCall (Name strs)))) i = ColAs (Exp $ ColCall $ Name strs) ((head $ reverse strs) ++ " (" ++ show (i+1) ++ ")")
postConvCol (Col val) i = ColAs val ("column (" ++ show (i+1) ++ ")")
postConvCol (ColAs val name) i = ColAs val (name ++ " (" ++ show (i+1) ++ ")")

{-
Helper functions for the conversion
-}

-- Gets the expression that calculates the probability
getProb :: Command -> [Bool] -> Expr
getProb (Select _ refs) bools
    | True `elem` bools = calcProb refs bools
    | otherwise = Num "1" -- prob is 1 if no probabilistic tables
getProb _ _ = Num "0"

-- Expression that calculates the probability
calcProb :: [Refine] -> [Bool] -> Expr
calcProb refs bools = FunCall "round" [typeCast, Exp (Num "3")] where
    typeCast = Type funProb "numeric"
    funProb = Exp $ FunCall "prob" [colDict, sentence] where
        colDict = dictCallBuilder (containsGroupBy refs)
        sentence = sentenceBuilder refs bools (containsGroupBy refs)

-- Condition that ensures the right (and only one) dictionary is used
dictCond :: Condition
dictCond = Compare colName Equal (Text dictName) where
        colName = Exp $ ColCall $ Name [dictTableName, dictNameCol]

-- Builds the call to the dictionary in the prob function
dictCallBuilder :: Bool -> Value
dictCallBuilder True = Exp $ FunCall "sum" [dictCallBuilder False] -- wrap everything in sum function
dictCallBuilder False =  Exp $ ColCall $ Name [dictTableName, dictDictCol]

-- Builds the sentence in the prob function
sentenceBuilder :: [Refine] -> [Bool] -> Bool -> Value
sentenceBuilder refs bools True = Exp $ FunCall "agg_or" [sentenceBuilder refs bools False] -- wrap everything in agg_or function
sentenceBuilder refs bools False
    | length exps > 0 = Exp $ Calc exp opExps -- more than one sentence
    | otherwise = Exp $ exp where -- just one sentence
        opExps = map (\ex -> (BAnd, ex)) exps -- combine the sentences
        (exp:exps) = map (\name -> ColCall name) sents
        sents = map (\(Name str) -> Name (str ++ [sentenceCol])) names -- add ._sentence to the table name
        names = getProbTables refs bools -- use every probabilistic table

-- Used to sort the Refine list after inserting WHERE
refSorter :: Refine -> Refine -> Ordering
refSorter a b
    | refMap a > refMap b = GT
    | otherwise = LT

-- Gets all tables in the FROM clause of a command
getTables :: Command -> [Table]
getTables (Select _ refs) = getRefTables refs
getTables _ = []

-- Gets the display name probabilistic tables in the FROM clause
getProbTables :: [Refine] -> [Bool] -> [Name]
getProbTables refs bools = map tableToDisplay tabs where
    tabs = keepIfTrue (getRefTables refs) bools

-- Gets all tables in the FROM clause
getRefTables :: [Refine] -> [Table]
getRefTables [] = []
getRefTables ((From tabConts):_) = contsToTables tabConts
getRefTables (_:xs) = getRefTables xs

-- List of all tables in a list of Table Containers
contsToTables :: [TabCont] -> [Table]
contsToTables tabConts = foldl (++) [] (map contToTables tabConts)

-- List of all tables in a single Table Container
contToTables :: TabCont -> [Table]
contToTables (Tabs tab joins) = (tab:tables) where
    tables = map (\(_,t,_) -> t) joins

-- Filter the elements that are marked as True
-- Used to only get the probabilistic tables
keepIfTrue :: [a] -> [Bool] -> [a]
keepIfTrue [] [] = [] -- ensures list are of equal length
keepIfTrue (x:xs) (b:bools)
    | b = (x:keepIfTrue xs bools)
    | otherwise = keepIfTrue xs bools

-- Get the list of display names of the tables in the FROM clause
getTablesDisplay :: [Refine] -> [Name]
getTablesDisplay [] = []
getTablesDisplay ((From tabConts):_) = map tableToDisplay (contsToTables tabConts)
getTablesDisplay (_:xs) = getTablesDisplay xs

-- Get the display name of a Table
tableToDisplay :: Table -> Name
tableToDisplay (Table name) = name
tableToDisplay (TableAs _ str) = Name [str]

-- Get the names of a list of columns
getColNames :: Command -> [String]
getColNames (Select cols _) = map getColName cols
getColNames cmd = []

-- Get the name of a column
getColName :: Col -> String
getColName (ColAs _ str) = str
getColName (Col _) = "none"
getColName (AllOf _) = "none"