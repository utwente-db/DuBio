module ErrorScanner where

import Grammar
import Printer

-- Scan the whole parse tree for errors
scanProgram :: [Command] -> Bool
scanProgram [] = True
scanProgram [Select cols refs] = scanCols cols && scanRefs refs
scanProgram (cmd:cmds) = scanProgram [cmd] && scanProgram cmds

scanCols :: [Col] -> Bool
scanCols [] = True
scanCols [AllOf (Name strs)]
    | lastNotStar || starNotLast = error $ "Column name not allowed: " ++ printName (Name strs)
    | otherwise = True where
        lastNotStar = strs!!(length strs - 1) /= "*"
        starNotLast = "*" `elem` (take (length strs - 1) strs)
scanCols [Col val] = scanVal val
scanCols [ColAs val _] = scanVal val
scanCols (col:cols) = scanCols [col] && scanCols cols

scanVal :: Value -> Bool
scanVal (Comm (Select cols refs)) = scanProgram [Select cols refs]
scanVal (Comm cmd) = error $ "Nested command not allowed: " ++ show cmd
scanVal _ = True -- TMP

scanRefs :: [Refine] -> Bool
scanRefs _ = True -- TMP