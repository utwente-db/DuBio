module DBConnector where
{-
Connects to a PostgreSQL database
And communicates with it
Author: Jochem Groot Roessink
-}

import Database.PostgreSQL.Simple
import Data.Aeson.Types
import Data.Aeson
import Data.String
import Data.Int
import Data.Word
import Control.Exception

import Grammar
import Names
import JSON

-- Establish a connection
connectDB :: IO Connection
connectDB = do
    putStr "Host: "
    host <- getLine
    putStr "Port: "
    port <- getLine
    putStr "Database: "
    db <- getLine
    putStr "User: "
    user <- getLine
    putStr "Password: "
    pass <- getLine
    let info = defaultConnectInfo {
          connectHost = host
        , connectPort = read port :: Word16
        , connectDatabase = db
        , connectUser = user
        , connectPassword = pass }
    conn <- connect info
    return conn

{-
Functions that help to determine for each table
whether they are probabilistic or not
(contain a sentence)
-}

-- Gets a booleans for every table
isProb :: Connection -> [Table] -> IO [Bool]
isProb conn tabs = do
    case tabs of
        [] -> return []
        tabs -> do
            bools <- (query_ conn (tablesToQuery tabs) :: IO [[Bool]])
            return (bools!!0)

-- Generate the query for the tables
tablesToQuery :: [Table] -> Query
tablesToQuery names = fromString $ "SELECT " ++ print where
    print = foldl (\a b -> a ++ ", " ++ b) first xs where
        (first:xs) = tabNamesToQStrings $ map tableToName names

-- Generate the subquery for a list of table names
tabNamesToQStrings :: [Name] -> [String]
tabNamesToQStrings [Name [table]] = [before ++ table ++ after] where
    before = "EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name='"
    after = "' AND column_name='" ++ sentenceCol ++ "')"
tabNamesToQStrings [Name (schema:[table])] = [before ++ schema ++ between ++ table ++ after] where
    before = "EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='"
    between = "' AND table_name='"
    after = "' AND column_name='" ++ sentenceCol ++ "')"
tabNamesToQStrings (x:xs) = tabNamesToQStrings [x] ++ tabNamesToQStrings xs

-- Get the name of a table
tableToName :: Table -> Name
tableToName (Table name) = name
tableToName (TableAs name _) = name

{-
Functions that send a command to the database
These only exist so that the result of a command can be displayed here
In the final product the generated SQL is sent to the database which
then displays the results
-}

-- Sends a query to the database and returns the result
sendQuery :: Connection -> String -> IO [[(String, String)]]
sendQuery conn str = do
    onlyJSON <- (query_ conn (stringToQuery str) :: IO [Only Data.Aeson.Value])
    let json = map (\(Only x) -> x) onlyJSON
    let encoded = map (show . encode) json
    let stringMap = map (run JSON.parseJSON) encoded
    return stringMap

-- Sends a non-query to the database and returns the number of rows affected
sendExec :: Connection -> String -> IO Int64
sendExec conn str = execute_ conn (fromString str)

-- Result needs to be JSON since Haskell needs to know the types in advance
stringToQuery :: String -> Query
stringToQuery str = fromString newStr where
    newStr = before ++ str ++ after where
        before = "SELECT row_to_json(generatedJSON) FROM ("
        after = ") AS generatedJSON"