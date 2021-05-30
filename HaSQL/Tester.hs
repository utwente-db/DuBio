module Tester where
{-
Test whether Parser and ErrorScanner return the desired result
Author: Jochem Groot Roessink
-}

import Control.Exception

import Grammar
import Parser
import ErrorScanner
import Converter
import Printer

-- Run all tests
allTests = do
    allParseTests
    allScanTests

{-
Test if the parser works as intended
-}

-- Parses the input and prints an error or the result
makeParseTest :: String -> String -> String -> IO ()
makeParseTest desc exp inp = do
    putStrLn "====================================================================\n"
    putStrLn $ "Running Test: " ++ desc
    putStrLn $ "Expected: " ++ exp
    putStrLn "Result:"
    parseRes <- try (evaluate (Parser.run parseProgram inp)) :: IO (Either SomeException [Command])
    case parseRes of
        Left msg -> putStrLn $ show msg
        Right cmds -> putStrLn $ show cmds
    putStr "\n"

-- Run all the parse tests
allParseTests = do
    doubleComparison
    columnNameIsFrom
    selectFromTable
    selectNumber
    selectCalc

doubleComparison = makeParseTest "Double comparison" "Should fail at the second equal sign" "select 3=2=1"
columnNameIsFrom = makeParseTest "Trying to select a column named \"from\"" "Should fail at from" "select from"
selectFromTable = makeParseTest "Select a column from a table" "Should parse correctly" "select id from person"
selectNumber = makeParseTest "Select a number" "Should parse correctly" "select 5"
selectCalc = makeParseTest "Select a calculation" "Should parse correctly" "select (3+2)*9"

{-
Test the error scanner works as intended
-}

-- Parses and scans the input and prints an error or the result
makeScanTest :: String -> String -> String -> IO ()
makeScanTest desc exp inp = do
    putStrLn "====================================================================\n"
    putStrLn $ "Running Test: " ++ desc
    putStrLn $ "Expected: " ++ exp
    putStrLn "Result:"
    parseRes <- try (evaluate (Parser.run parseProgram inp)) :: IO (Either SomeException [Command])
    case parseRes of
        Left msg -> putStrLn $ show msg
        Right tree -> do
            res <- try (evaluate (scanProgram tree)) :: IO (Either SomeException Bool)
            case res of
                Left msg -> putStrLn $ show msg
                Right False -> putStrLn "Unknown error detected during parse tree scan"
                Right True -> putStrLn "Accepted"

-- Run all the scan tests
allScanTests = do
    doubleFrom
    incorrectOrder
    correctOrder
    subCommandNotQuery
    subCommandIsQuery

doubleFrom = makeScanTest "Double FROM" "Should fail because two FROM statements" "select id from person from person2"
incorrectOrder = makeScanTest "Incorrect order in SELECT statement" "Should fail because of the wrong order" "select id where id > 3 from person"
correctOrder = makeScanTest "Correct order in SELECT statement" "Should be accepted" "select id from person where id > 3"
subCommandNotQuery = makeScanTest "Subcommand is not a query" "Should fail because subcommand cannot be a query" "select (select 3 into table)"
subCommandIsQuery = makeScanTest "Subcommand is a query" "Should be accepted" "select (select 3)"