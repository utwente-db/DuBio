module Converter where

import Grammar
import Names
import Data.List

preConv :: [Command] -> [Command]
preConv [None] = []
preConv [cmd] = [cmd]
preConv (cmd:xs) = preConv [cmd] ++ preConv xs

convCommand :: Command -> Expr -> Command
convCommand (Select cols refs) prob
    | prob == (Num "100") = Select cols refs -- no conversion needed
    | otherwise = Select (convCols newCols prob) (convRefs refs prob) where
        newCols = convAll cols names
        names = map tableToDisplay (getRefTables refs)
convCommand None _ = None

convAll :: [Col] -> [Name] -> [Col]
convAll [] _ = []
convAll (AllOf (Name ["*"]):xs) names = newAll ++ xs where
    newAll = map (\n -> AllOf n) cols
    cols = map (\(Name xs) -> Name (xs++["*"])) names
convAll (col:xs) names = (col:convAll xs names)

convCols :: [Col] -> Expr -> [Col]
convCols cols prob = map (flip convCol prob) cols ++ [ColAs (Exp prob) (probName ++ " %")]

convCol :: Col -> Expr -> Col
convCol (ColAs val name) prob = ColAs (convValue val prob) name
convCol (Col val) prob = Col (convValue val prob)
convCol col _ = col

convValue :: Value -> Expr -> Value
convValue (Exp exp) prob = Exp $ convExpr exp prob
convValue (Cond cond) prob = Cond $ convCond cond prob
convValue val _ = val

convExpr :: Expr -> Expr -> Expr
convExpr (ColCall (Name [name])) prob | name == probAttr = prob
convExpr (Calc exp opExps) prob = Calc (convExpr exp prob) (map func opExps) where
    func = (\(a,b) -> (a, convExpr b prob))
convExpr exp _ = exp

convCond :: Condition -> Expr -> Condition
convCond (And cond conds) prob = And (convCond cond prob) (map (flip convCond prob) conds)
convCond (Or cond conds) prob = Or (convCond cond prob) (map (flip convCond prob) conds)
convCond (Not cond) prob = Not (convCond cond prob)
convCond (Compare val1 op val2) prob = Compare (convValue val1 prob) op (convValue val2 prob)
convCond (Between val1 val2 val3) prob = Between (c val1) (c val2) (c val3) where
    c val = convValue val prob
convCond cond _ = cond

convRefs :: [Refine] -> Expr -> [Refine]
convRefs refs prob
    | containsWhere refs = map (flip convRef prob) refs
    | otherwise = sortBy refSorter (Where dictCond:map (flip convRef prob) refs)

convRef :: Refine -> Expr -> Refine
convRef (From tabConts) _ = From (convTabConts tabConts)
convRef (Where cond) prob = Where $ convCond (convWhereCond cond) prob
convRef (OrderBy exp ord) prob = OrderBy (convExpr exp prob) ord
convRef ref _ = ref

convTabConts :: [TabCont] -> [TabCont]
convTabConts conts = conts ++ [(Tabs (Table (Name ["_dict"])) [])]

convWhereCond :: Condition -> Condition
convWhereCond (And cond xs) = And cond (xs ++ [dictCond])
convWhereCond cond = And cond [dictCond]

postConv :: Command -> Command
postConv (Select cols refs) = Select newCols refs where
    newCols = [postConvCol col i | i <- [0..length cols-1], let col = cols!!i]
postConv cmd = cmd

postConvCol :: Col -> Int -> Col
postConvCol (AllOf name) i = AllOf name
postConvCol (Col (Exp (FunCall name vals))) i = ColAs (Exp $ FunCall name vals) (name ++ " (" ++ show (i+1) ++ ")")
postConvCol (Col (Exp (ColCall (Name strs)))) i = ColAs (Exp $ ColCall $ Name strs) ((head $ reverse strs) ++ " (" ++ show (i+1) ++ ")")
postConvCol (Col val) i = ColAs val ("column (" ++ show (i+1) ++ ")")
postConvCol (ColAs val name) i = ColAs val (name ++ " (" ++ show (i+1) ++ ")")

getProb :: Command -> [Bool] -> Expr
getProb (Select _ refs) bools
    | True `elem` bools = calcProb refs bools
    | otherwise = Num "100"
getProb _ _ = Num "0"

calcProb :: [Refine] -> [Bool] -> Expr
calcProb refs bools = FunCall "round" [prob100] where
    prob100 = Exp $ Calc funProb [(Multiply, Num "100")]
    funProb = FunCall "prob" [colDict, sentence] where
        colDict = dictCallBuilder (containsGroupBy refs)
        sentence = sentenceBuilder refs bools (containsGroupBy refs)

dictCond :: Condition
dictCond = Compare colName Equal (Text dictName) where
        colName = Exp $ ColCall $ Name [dictTableName, dictNameCol]

dictCallBuilder :: Bool -> Value
dictCallBuilder True = Exp $ FunCall "sum" [dictCallBuilder False]
dictCallBuilder False =  Exp $ ColCall $ Name [dictTableName, dictDictCol]

sentenceBuilder :: [Refine] -> [Bool] -> Bool -> Value
sentenceBuilder refs bools True = Exp $ FunCall "agg_or" [sentenceBuilder refs bools False]
sentenceBuilder refs bools False
    | length exps > 0 = Exp $ Calc exp opExps
    | otherwise = Exp $ exp where
        opExps = map (\ex -> (BAnd, ex)) exps
        (exp:exps) = map (\name -> ColCall name) sents
        sents = map (\(Name str) -> Name (str ++ [sentenceCol])) names
        names = getProbTables refs bools

containsWhere :: [Refine] -> Bool
containsWhere [] = False
containsWhere (Where _:_) = True
containsWhere (_:xs) = containsWhere xs

containsGroupBy :: [Refine] -> Bool
containsGroupBy [] = False
containsGroupBy (GroupBy _:_) = True
containsGroupBy (_:xs) = containsGroupBy xs

refSorter :: Refine -> Refine -> Ordering
refSorter a b
    | refMap a > refMap b = GT
    | otherwise = LT

getTables :: Command -> [Table]
getTables (Select _ refs) = getRefTables refs
getTables _ = []

getProbTables :: [Refine] -> [Bool] -> [Name]
getProbTables refs bools = map tableToDisplay tabs where
    tabs = keepIfTrue (getRefTables refs) bools

getRefTables :: [Refine] -> [Table]
getRefTables [] = []
getRefTables ((From tabConts):_) = contsToTables tabConts
getRefTables (_:xs) = getRefTables xs

contsToTables :: [TabCont] -> [Table]
contsToTables tabConts = foldl (++) [] (map contToTables tabConts)

contToTables :: TabCont -> [Table]
contToTables (Tabs tab joins) = (tab:tables) where
    tables = map (\(_,t,_) -> t) joins

keepIfTrue :: [a] -> [Bool] -> [a]
keepIfTrue [] [] = [] -- ensures list are of equal length
keepIfTrue (x:xs) (b:bools)
    | b = (x:keepIfTrue xs bools)
    | otherwise = keepIfTrue xs bools

getTablesDisplay :: [Refine] -> [Name]
getTablesDisplay [] = []
getTablesDisplay ((From tabConts):_) = map tableToDisplay (contsToTables tabConts)
getTablesDisplay (_:xs) = getTablesDisplay xs

tableToDisplay :: Table -> Name
tableToDisplay (Table name) = name
tableToDisplay (TableAs _ str) = Name [str]

getColNames :: Command -> [String]
getColNames (Select cols _) = map getColName cols
getColNames cmd = []

getColName :: Col -> String
getColName (ColAs _ str) = str
getColName (Col _) = "none"
getColName (AllOf _) = "none"