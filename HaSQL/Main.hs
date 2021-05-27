module Main where

import Control.Exception
import Text.Printf

import Grammar
import Parser
import ErrorScanner
import Converter
import Printer
import DBConnector
import JSON

main = do
    conn <- connectDB
    loop conn

loop conn = do
    putStrLn ">>> Enter your pSQL commands (or \"q\" to quit):"
    inp <- getLines
    inner conn inp

inner conn inp
    | inp /= "q\n" = do
        eval conn inp
        loop conn
    | otherwise = do
        putStrLn ">>> Quit"

eval conn inp = do
    parseRes <- try (evaluate (Parser.run parseProgram inp)) :: IO (Either SomeException [Command])
    case parseRes of
        Left msg -> putStrLn $ show msg
        Right tree -> do
            res <- try (evaluate (scanProgram tree)) :: IO (Either SomeException Bool)
            case res of
                Left msg -> putStrLn $ show msg
                Right True -> mapM_ (evalCommand conn) (preConv tree)
                Right False -> putStrLn "Unknown error detected during scan"

getLines :: IO String
getLines = do
    x <- getLine
    if x == "" then
        return ""
    else do
        xs <- getLines
        return (x++"\n"++xs)

prettyPrint :: [String] -> IO ()
prettyPrint vals = putStrLn . unwords $ printf "%-20.20s" <$> vals

-- evalCommand :: ConnecCommand -> IO ()
evalCommand conn cmd = do
    tablesProb <- isProb conn $ getTables cmd
    putStrLn ">>> Generated SQL Command:"
    let prob = getProb cmd tablesProb
    let userTree = convCommand cmd prob
    let userSQL = printProgram [userTree]
    putStr userSQL
    let tree = postConv userTree
    let sql = printProgram [tree]
    let reqCols = getColNames tree
    json <- getJSON conn sql
    putStrLn ">>> Result:"
    if length json == 0 then
        putStrLn "No rows found\n"
    else do
        let ordered = map (orderLike reqCols) json
        let columns = map (\(a,b) -> a) (ordered!!0)
        let rows = map (map (\(a,b) -> b)) ordered
        prettyPrint columns
        mapM_ prettyPrint rows
        putStrLn ""