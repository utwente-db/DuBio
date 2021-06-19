module ErrorScanner where
{-
Scans the whole parse tree for errors
throws one if detected, returns True otherwise.
Not very strict since the SQL compiler will
check for errors as well.
Author: Jochem Groot Roessink
-}

import Grammar
import GrammarFunctions
import Printer

{-
Scanners that scan the parse tree
-}

-- Scan the whole parse tree for errors
scanProgram :: [Command] -> Bool
scanProgram [] = True
scanProgram [Select _ cols refs] = scanCols cols && scanRefs refs
scanProgram [cmd] = True
scanProgram (cmd:cmds) = scanProgram [cmd] && scanProgram cmds

-- Scans every Column for errors
scanCols :: [Col] -> Bool
scanCols [] = True
scanCols [AllOf (Name strs)]
    | lastNotStar || starNotLast = error $ "Column name not allowed: " ++ printName (Name strs)
    | otherwise = True where
        lastNotStar = strs!!(length strs - 1) /= "*" -- should always end in *: *, person.*
        starNotLast = "*" `elem` (take (length strs - 1) strs) -- * can only be the last element
scanCols [Col val] = scanValue val
scanCols [ColAs val _] = scanValue val
scanCols (col:cols) = scanCols [col] && scanCols cols

-- Scan the list of Refine datatypes
scanRefs :: [Refine] -> Bool
scanRefs [] = True
scanRefs refs
    | not $ isUnique orderNums = error "Double substatement in SELECT statement"
    | not $ foldl (&&) True isHigher = error "Incorrect order of substatements in SELECT statement"
    | otherwise = foldl (&&) True (map scanRef refs) where
        isHigher = zipWith (<) prevNums orderNums
        prevNums = (-1:reverse (tail $ reverse orderNums))
        orderNums = map refMap refs

-- Scan a single Refine datatype
scanRef :: Refine -> Bool
scanRef (Where cond) = scanCond cond
scanRef (Having cond) = scanCond cond
scanRef _ = True

-- Scan a condition
scanCond :: Condition -> Bool
scanCond (Compare val1 op val2) = scanValue val1 && scanValue val2
scanCond (Between ex1 ex2 ex3) = foldl (&&) True (map scanExpr [ex1, ex2, ex3])
scanCond (And cond conds) = foldl (&&) (scanCond cond) (map scanCond conds)
scanCond (Or cond conds) = foldl (&&) (scanCond cond) (map scanCond conds)
scanCond (Not cond) = scanCond cond
scanCond _ = True

-- Scan a value
scanValue :: Value -> Bool
scanValue (Comm cmd)
    | not $ isQuery cmd = error $ "Nested command can only be a query"
    | otherwise = True
scanValue (Tuple vals) = foldl (&&) True (map scanValue vals)
scanValue (Type val _) = scanValue val
scanValue _ = True

-- Scan an expression
scanExpr :: Expr -> Bool
scanExpr _ = True

{-
Helper functions
-}

-- Checks whether all elements in a list are unique
isUnique :: Eq a => [a] -> Bool
isUnique [] = True
isUnique (x:xs)
    | x `elem` xs = False
    | otherwise = isUnique xs