module Main where
{-
Asks for user input
Then parses, scans for error, converts and prints the new SQL
Can also send the query to the database
-}

import Database.PostgreSQL.Simple
import Control.Exception
import Text.Printf
import Data.Char
import Data.Time.Clock.POSIX

import Grammar
import GrammarFunctions
import Parser
import ErrorScanner
import Converter
import Printer
import DBConnector
import JSON

-- Connect to database and start loop
main :: IO ()
main = do
    conn <- connectDB
    loop conn

-- Ask for user input and call execOrQuit
loop :: Connection -> IO ()
loop conn = do
    putStrLn ">>> Enter your inSQeLto commands (or \"q\" to quit):"
    inp <- getLines
    execOrQuit conn inp

-- Either execute the input and ask for new input or quit
execOrQuit :: Connection -> String -> IO ()
execOrQuit conn inp
    | inp /= "q\n" = do
        exec conn inp
        loop conn
    | otherwise = do
        putStrLn ">>> Quit"

-- Parse, scan for errors, query database which tables are probabilistic,
-- convert, and print
exec :: Connection -> String -> IO ()
exec conn inp = do
    --startTime <- round . (1000 *) <$> getPOSIXTime
    parseRes <- try (evaluate (Parser.run parseProgram inp)) :: IO (Either SomeException [Command])
    case parseRes of
        Left msg -> putStrLn $ show msg
        Right tree -> do
            res <- try (evaluate (scanProgram tree)) :: IO (Either SomeException Bool)
            case res of
                Left msg -> putStrLn $ show msg
                Right False -> putStrLn "Unknown error detected during parse tree scan"
                Right True -> do
                    let preConvTree = preConv tree
                    isProbList <- mapM (getIsProbList conn) preConvTree
                    let convTree = convProgram preConvTree isProbList
                    putStrLn ">>> Generated SQL Commands:"
                    putStrLn $ printProgram convTree
                    --endTime <- round . (1000 *) <$> getPOSIXTime
                    --putStrLn $ show $ endTime - startTime
                    -- Print the results per command
                    putStr "Execute the commands (Y/N)? "
                    answer <- getLine
                    if map toUpper answer == "Y" then do
                        mapM_ (execCommand conn) convTree
                    else do
                        putStrLn ""

-- Convert a command and send it to the database
-- And print the results
execCommand :: Connection -> Command -> IO ()
execCommand conn cmd = do
    putStrLn ">>> Executing the following command:"
    case cmd of
        Other _ -> putStrLn "(This command was not converted)"
        _ -> putStr ""
    putStrLn $ printProgram [cmd]
    let tree = postConv cmd
    let sql = printProgram [tree]
    if isQuery cmd then do
        putStrLn ">>> Result:"
        json <- sendQuery conn sql
        if length json == 0 then
            putStrLn "No rows found\n"
        else do
            let reqCols = getColNames tree
            let ordered = map (orderLike reqCols) json
            let columns = map (\(a,b) -> a) (ordered!!0)
            let rows = map (map (\(a,b) -> b)) ordered
            prettyPrint columns
            mapM_ prettyPrint rows
            putStrLn ""
    else do
        rowsAffected <- sendExec conn sql
        putStrLn ">>> Rows affected:"
        putStrLn $ show rowsAffected ++ "\n"


-- Get multiple lines of input
getLines :: IO String
getLines = do
    x <- getLine
    if x == "" then
        return ""
    else do
        xs <- getLines
        return (x++"\n"++xs)

-- Get the boolean list for which tables are probabilistic
getIsProbList :: Connection -> Command -> IO [Bool]
getIsProbList conn cmd = do
    isProbList <- isProb conn $ getTables cmd
    return isProbList

-- Pretty print a list of strings
prettyPrint :: [String] -> IO ()
prettyPrint vals = putStrLn . unwords $ printf "%-20.20s" <$> vals