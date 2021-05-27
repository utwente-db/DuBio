module JSON where

import Text.ParserCombinators.Parsec
import Text.ParserCombinators.Parsec.Language
import qualified Text.ParserCombinators.Parsec.Token as Token

languageDef =
    emptyDef { Token.reservedOpNames = []
             , Token.reservedNames = []
    }

lexer = Token.makeTokenParser languageDef
braces = Token.braces lexer
symbol = Token.symbol lexer

parseJSON :: Parser [(String, String)]
parseJSON = between (char '\"') (char '\"') str where
    str = braces $ sepBy parseTuple (symbol ",")

parseTuple :: Parser (String, String)
parseTuple = (,) <$> (parseString <* symbol ":") <*> parseString

parseString :: Parser String
parseString = par <|> nopar <|> tup where
    par = try $ between (symbol "\\\"") (symbol "\\\"") (many $ noneOf "\\\"")
    nopar = try $ many1 $ noneOf "\\\":,{}"
    tup = braces $ many1 $ noneOf "}"

orderLike :: [String] -> [(String, String)] -> [(String, String)]
orderLike cols json = rest ++ known where
    rest = filter (\(a,_) -> not (a `elem` cols)) json
    known = [tup | col <- cols, let tups = byCol col, length tups == 1, let tup = head tups]
    byCol col = filter (\(a,_) -> a == col) json

--- Function that uses a given parser to parse a given string
run :: Parser a -> String -> a
run p xs = case res of
    Left x -> error $ show $ x
    Right x -> x
    where res = parse p "" xs