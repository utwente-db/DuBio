module Printer where

import Grammar

--- Printing here means converting to a string

-- Prints the program
printProgram :: [Command] -> String
printProgram cmds = printCommands cmds ++ "\n\n"

-- Prints a list of the Command datatype
printCommands :: [Command] -> String
printCommands [Select cols refs] = "SELECT " ++ printCols cols ++ printRefs refs
printCommands [None] = ""
printCommands (cmd:xs) = printCommands [cmd] ++ ";\n" ++ printCommands xs

-- Prints a list of the Col(umn) datatype
printCols :: [Col] -> String
printCols [AllOf (Name ["*"])] = "*"
printCols [AllOf (Name strs)] = printName (Name newStrs) ++ ".*" where
    newStrs = take (length strs - 1) strs
printCols [Col val] = printValues [val] Zero
printCols [ColAs val str] = printValues [val] Zero ++ " AS " ++ printWords [str]
printCols (col:xs) = printCols [col] ++ ", " ++ printCols xs

-- Prints a list of the Refine datatype
printRefs :: [Refine] -> String
printRefs [] = ""
printRefs [From tabConts] = "\nFROM " ++ printTabConts tabConts
printRefs [Where cond] = "\nWHERE " ++ printConds [cond] Zero
printRefs [GroupBy name] = "\nGROUP BY " ++ printName name
printRefs [OrderBy exp ord] = "\nORDER BY " ++ printExpr exp Zero ++ " " ++ printOrder ord
printRefs [Limit num] = "\nLIMIT " ++ num
printRefs (ref:xs) = printRefs [ref] ++ printRefs xs

-- Prints the Order datatype (Asc, Desc)
printOrder :: Order -> String
printOrder Asc = "ASC"
printOrder Desc = "DESC"

-- Prints a list of the TabCont (TableContainer) datatype
printTabConts :: [TabCont] -> String
printTabConts [Tabs tab joins] = printTable tab ++ printJoinTuples joins
printTabConts (tab:xs) = printTabConts [tab] ++ ", " ++ printTabConts xs

-- Prints a list of (Join, Table, Condition) tuples
printJoinTuples :: [(Join, Table, Condition)] -> String
printJoinTuples [] = ""
printJoinTuples [(j,t,c)] = " " ++ printJoin j ++ " " ++ printTable t ++ " ON " ++ printConds [c] Zero
printJoinTuples (tup:xs) = printJoinTuples [tup] ++ printJoinTuples xs

-- Prints the Join datatype
printJoin :: Join -> String
printJoin Inner = "JOIN"
printJoin Lft = "LEFT JOIN"
printJoin Rght = "RIGHT JOIN"
printJoin Full = "FULL"

-- Prints a list of the Table datatype
printTable :: Table -> String
printTable (Table name) = printName name
printTable (TableAs name str) = printName name ++ " AS " ++ printWords [str]

-- Prints a list of the Value datatype
printValues :: [Value] -> Operator -> String
printValues [Text str] _ = "'" ++ str ++ "'"
printValues [Exp expr] parentOp = printExpr expr parentOp
printValues [Cond cond] parentOp = printConds [cond] parentOp
printValues [Tuple [val]] parentOp = printValues [val] parentOp
printValues [Tuple vals] _ = "(" ++ printValues vals Zero ++ ")"
printValues [Case condVals elseVal] _ = "CASE" ++ print ++ printedElseVal ++ "\nEND" where
    print = foldl (++) "" printedCondVals
    printedCondVals = map func condVals
    func = (\(c,v) -> "\nWHEN " ++ printConds [c] Zero ++ " THEN " ++ printValues [v] Zero)
    printedElseVal = case elseVal of
        Nothing -> ""
        Just val -> "\nELSE " ++ printValues [val] Zero
printValues [Comm cmd] _ = "(" ++ printCommands [cmd] ++ ")"
printValues (val:xs) _ = printValues [val] Zero ++ "," ++ printValues xs Zero

-- Prints the Expr(ession) datatype
printExpr :: Expr -> Operator -> String
printExpr (Calc expr pairs) parentOp = subVals (Exp expr) valPairs parentOp where
    valPairs = map func pairs
    func = (\(o, ex) -> (o, Exp ex))
printExpr (Num str) _ = str
printExpr (ColCall name) _ = printName name
printExpr (FunCall str vals) _ = printWords [str] ++ "(" ++ printValues vals Zero ++ ")"

-- Prints a list of the Condition datatype
printConds :: [Condition] -> Operator -> String
printConds [IsTrue] _ = "TRUE"
printConds [IsFalse] _ = "FALSE"
printConds [And cond xs] parentOp = subVals (Cond cond) pairs parentOp where
    pairs = map (\cnd -> (CAnd, Cond cnd)) xs
printConds [Or cond xs] parentOp = subVals (Cond cond) pairs parentOp where
    pairs = map (\cnd -> (COr, Cond cnd)) xs
printConds [Not cond] _ = "NOT " ++ printConds [cond] CNot
printConds [Compare val1 op val2] parentOp
    | level > parentLevel = print
    | otherwise = "(" ++ print ++ ")" where
        print = printValues [val1] op ++ printIcon op ++ printValues [val2] op
        level = getLevel op -- get the level of the operator
        parentLevel = getLevel parentOp -- get the level of the parent operator
printConds [Between val1 val2 val3] parentOp = p val1 ++ " BETWEEN " ++ p val2 ++ " AND " ++ p val3 where
    p val = printValues [val] parentOp
printConds (cond:xs) op = printConds [cond] op ++ printIcon op ++ printConds xs op

-- Helper function for printExpr & printConds
subVals :: Value -> [(Operator, Value)] -> Operator -> String
subVals (Exp expr) pairs parentOp
    | level > parentLevel = printExpr expr op ++ rest -- no parentheses needed
    | otherwise = "(" ++ printExpr expr op ++ rest ++ ")" where -- parentheses needed
        rest = foldl (++) "" printedPairs -- prints all the operator-expression pairs
        printedPairs = map func pairs
        func = (\(o, (Exp ex)) -> printIcon o ++ printExpr ex o)
        level = getLevel op -- get the level of the first operator
        op = (\(o,_) -> o) $ pairs!!0 -- get the first operator
        parentLevel = getLevel parentOp -- get the level of the parent operator
subVals (Cond cond) pairs parentOp
    | level > parentLevel = print -- no parentheses needed
    | otherwise = "(" ++ print ++ ")" where -- parentheses needed
        print = printConds (cond:conds) op -- prints the subconditons
        conds = map (\(_, (Cond cnd)) -> cnd) pairs -- gets a list of all the subconditions
        level = getLevel op -- get the level of the operator
        op = (\(o,_) -> o) $ pairs!!0 -- get the operator
        parentLevel = getLevel parentOp -- get the level of the parent operator

-- Prints the Name datatype
printName :: Name -> String
printName (Name wrds) = printWords wrds

forbidden :: String
forbidden = " \t\n.,\"();*+-=<>!&|^%"

-- Helper function for printName
printWords :: [String] -> String
printWords [wrd]
    | containsOneOf wrd forbidden = "\"" ++ wrd ++ "\""
    | otherwise = wrd
printWords (wrd:xs) = printWords [wrd] ++ "." ++ printWords xs

-- Helper function for printWords
containsOneOf :: String -> String -> Bool
containsOneOf text chars = foldl (||) False containsChar where
    containsChar = map (\c -> c `elem` text) chars

-- Prints the icon of an operator
printIcon :: Operator -> String
printIcon op
    | level > 0 && level < 5 = " " ++ icon ++ " " -- surround with whitespace
    | otherwise = icon where -- don't surround with whitespace
        level = getLevel op
        icon = getIcon op