module DBConnector where

import Database.PostgreSQL.Simple
import Data.Aeson.Types
import Data.Aeson
import Data.String
import Data.Word

import Grammar
import Names
import JSON

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

getJSON :: Connection -> String -> IO [[(String, String)]]
getJSON conn str = do
    onlyJSON <- (query_ conn (stringToQuery str) :: IO [Only Data.Aeson.Value])
    let json = map (\(Only x) -> x) onlyJSON
    let encoded = map (show . encode) json
    let stringMap = map (run JSON.parseJSON) encoded
    return stringMap

getJSON' :: String -> IO String
getJSON' str = do
    conn <- connect localPG
    onlyJSON <- (query_ conn (stringToQuery str) :: IO [Only Data.Aeson.Value])
    let json = map (\(Only x) -> x) onlyJSON
    let encoded = map (show . encode) json
    return $ encoded!!0

isProb :: Connection -> [Table] -> IO [Bool]
isProb conn tabs = do
    case tabs of
        [] -> return []
        tabs -> do
            bools <- (query_ conn (tablesToQuery tabs) :: IO [[Bool]])
            return (bools!!0)

tablesToQuery :: [Table] -> Query
tablesToQuery names = fromString $ "SELECT " ++ print where
    print = foldl (\a b -> a ++ ", " ++ b) first xs where
        (first:xs) = tabNamesToQStrings $ map tableToName names

tabNamesToQStrings :: [Name] -> [String]
tabNamesToQStrings [Name [table]] = [before ++ table ++ after] where
    before = "EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name='"
    after = "' AND column_name='" ++ sentenceCol ++ "')"
tabNamesToQStrings [Name (schema:[table])] = [before ++ schema ++ between ++ table ++ after] where
    before = "EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='"
    between = "' AND table_name='"
    after = "' AND column_name='" ++ sentenceCol ++ "')"
tabNamesToQStrings (x:xs) = tabNamesToQStrings [x] ++ tabNamesToQStrings xs

tableToName :: Table -> Name
tableToName (Table name) = name
tableToName (TableAs name _) = name

stringToQuery :: String -> Query
stringToQuery str = fromString newStr where
    newStr = before ++ str ++ after where
        before = "SELECT row_to_json(generatedJSON) FROM ("
        after = ") AS generatedJSON"