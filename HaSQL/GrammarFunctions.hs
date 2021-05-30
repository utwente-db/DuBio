module GrammarFunctions where
{-
Functions that act on the grammar from Grammar.hs
and are used in multiple other files
Author: Jochem Groot Roessink
-}

import Grammar

-- Used to decide whether to call query or execute command
isQuery :: Command -> Bool
isQuery (Select _ refs) = not $ containsInto refs
isQuery (Other _) = False

-- Used to check the order of the list of refine elements
-- Don't use negative numbers here
refMap :: Refine -> Int
refMap (Into _) = 0
refMap (From _) = 1
refMap (Where _) = 2
refMap (GroupBy _) = 3
refMap (OrderBy _ _) = 4
refMap (Limit _) = 5

-- Used to decide whether operation should be printed with parentheses
-- and what symbol(s) to parse/print
operatorMap :: Operator -> (Int, [String])
operatorMap Zero = (0, [])
operatorMap COr = (1, ["OR"])
operatorMap CAnd = (2, ["AND"])
operatorMap CNot = (3, ["NOT"])
operatorMap Equal = (4, ["="])
operatorMap Greater = (4, [">"])
operatorMap Less = (4, ["<"])
operatorMap GreatEq = (4, [">="])
operatorMap LessEq = (4, ["<="])
operatorMap NotEq = (4, ["<>", "!="])
operatorMap Like = (4, ["LIKE"])
operatorMap In = (4, ["IN"])
operatorMap BAnd = (5, ["&"])
operatorMap BOr = (5, ["|"])
operatorMap Add = (6, ["+"])
operatorMap Subtract = (6, ["-"])
operatorMap Modulo = (6, ["%"])
operatorMap Multiply = (7, ["*"])
operatorMap Divide = (7, ["/"])
operatorMap Power = (8, ["^"])

-- Gets all symbols related to an operator
getIcons :: Operator -> [String]
getIcons op = (\(_, strs) -> strs) $ operatorMap op

-- Gets the first symbol related to an operator
getIcon :: Operator -> String
getIcon op = (getIcons op)!!0

-- Gets the print level of an operator
-- If the level < parentLevel parentheses are needed
-- (3+2)*5, but 3*2+5
getLevel :: Operator -> Int
getLevel op = (\(l, _) -> l) $ operatorMap op

-- Checks whether there is a WHERE statement
containsWhere :: [Refine] -> Bool
containsWhere [] = False
containsWhere (Where _:_) = True
containsWhere (_:xs) = containsWhere xs

-- Checks whether there is a GROUP BY statement
containsGroupBy :: [Refine] -> Bool
containsGroupBy [] = False
containsGroupBy (GroupBy _:_) = True
containsGroupBy (_:xs) = containsGroupBy xs

-- Checker whether there is an INTO statement
containsInto :: [Refine] -> Bool
containsInto [] = False
containsInto (Into _:_) = True
containsInto (_:xs) = containsInto xs

-- List of characters that cannot be in an identifier
forbidden :: String
forbidden = " \t\n.,\"();*+-=<>!&|^%:"