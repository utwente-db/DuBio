module Tester where

import Main

runTest desc input = do
    putStrLn "====================================================================\n"
    putStrLn $ "Running Test: " ++ desc ++ "\nWhich returns:"
    eval input
    putStr "\n"

--- Run all tests
runAllTests = do
    runParseTests

--- Test Parser
runParseTests = do
    doubleComparison
    columnNameIsFrom

doubleComparison = runTest "Double comparison" "select 3=2=1"
-- should fail parse at the second equal sign

columnNameIsFrom = runTest "Trying to select a column named \"from\"" "select from"
-- should fail at from