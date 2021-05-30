module Parser where
{-
Parses text input to a parse tree from Grammar.hs
Author: Jochem Groot Roessink
-}

import Text.ParserCombinators.Parsec
import Text.ParserCombinators.Parsec.Language
import qualified Text.ParserCombinators.Parsec.Token as Token

import Grammar
import GrammarFunctions

import Data.Char

languageDef =
    emptyDef { Token.identStart      = noneOf forbidden 
             , Token.identLetter     = noneOf forbidden             
             , Token.caseSensitive = False
             , Token.reservedOpNames = 
                [ "=", "!=", "<>"
                , "<=", ">=", ">", "<"
                , "&", "|", "^"
                , "+", "-", "%"
                , "*", "/"
                ]
             , Token.reservedNames =
                [ "SELECT"
                , "AS", "ON", "BY"
                , "FROM", "WHERE"
                , "ORDER", "GROUP"
                , "LIMIT", "INTO"
                , "AND", "OR", "NOT"
                , "TRUE", "FALSE"
                , "DESC", "ASC"
                , "CASE", "WHEN", "THEN", "END"
                , "LIKE", "IN", "BETWEEN"
                , "JOIN", "INNER", "OUTER"
                , "LEFT", "RIGHT", "FULL"
                ]
    }

lexer = Token.makeTokenParser languageDef
lexeme = Token.lexeme lexer
identifier = Token.identifier lexer
parens = Token.parens lexer
braces = Token.braces lexer
symbol = Token.symbol lexer
reserved = Token.reserved lexer
whitespace = Token.whiteSpace lexer

{-
Functions that parse the grammar from Grammar.hs
-}

-- Parses a sequence of commands
parseProgram :: Parser [Command]
parseProgram = sepBy1 (optional (lexeme whitespace) *> parseCommand) (symbol ";") <* eof

-- Parses the Command data type
parseCommand :: Parser Command
parseCommand = select <|> other where
    select = (Select <$ reserved "SELECT") <*> sepBy1 parseCol (symbol ",") <*> many parseRefine
    other = Other <$> many (single <|> double <|> rest) where
        single = (\a b c -> [a] ++ b ++ [c]) <$> (char '\'') <*> (many $ noneOf "\'") <*> (char '\'') -- single quotes
        double = (\a b c -> [a] ++ b ++ [c]) <$> (char '\"') <*> (many $ noneOf "\"") <*> (char '\"') -- double quotes
        rest = many1 $ noneOf ";\"\'" -- no quotes, can not contain ;

-- Parses the Col(umn) data type
parseCol :: Parser Col
parseCol = colAs <|> col <|> allOf where
    colAs = try $ ColAs <$> parseValue True <*> ((optional $ reserved "AS") *> parseWord)
    col = try $ Col <$> parseValue True
    allOf = AllOf <$> parseAllOf

-- Parses the TabCont (Table Container) data type
parseTabCont :: Parser TabCont
parseTabCont = Tabs <$> parseTable <*> many ((,,) <$> parseJoin <*> parseTable <*> (reserved "ON" *> parseCondition))

-- Parser the Join data type
parseJoin :: Parser Join
parseJoin = join <|> left <|> right <|> full where
    join = Inner <$ (optional $ reserved "INNER") <* reserved "JOIN"
    left = Lft <$ reserved "LEFT" <* (optional $ reserved "OUTER") <* reserved "JOIN"
    right = Rght <$ reserved "RIGHT" <* (optional $ reserved "OUTER") <* reserved "JOIN"
    full = Full <$ reserved "FULL" <* (optional $ reserved "OUTER") <* reserved "JOIN"

-- Parses the Table data type
parseTable :: Parser Table
parseTable = tableAs <|> table where
    tableAs = try $ TableAs <$> parseName <*> ((optional $ reserved "AS") *> parseWord)
    table = Table <$> parseName

-- Parser the Name data type
parseName :: Parser Name
parseName = Name <$> sepBy1 parseWord (symbol ".")

-- Same as parseName but can also parse * between the dots
parseAllOf :: Parser Name
parseAllOf = Name <$> sepBy1 (word <|> star) (symbol ".") where
    word = parseWord
    star = symbol "*"

-- Parses the Refine data type
parseRefine :: Parser Refine
parseRefine = from <|> whr <|> group <|> limit <|> into <|> orderDesc <|> orderAsc where
    from = (From <$ reserved "FROM") <*> sepBy1 parseTabCont (symbol",")
    whr = (Where <$ reserved "WHERE") <*> parseOr
    group = (GroupBy <$ reserved "GROUP") <*> (reserved "BY" *> lexeme parseName)
    limit = (Limit <$ reserved "LIMIT") <*> lexeme (many1 (oneOf "0123456789"))
    into = (Into <$ reserved "INTO") <*> lexeme parseName
    orderDesc = try $ (OrderBy <$ reserved "ORDER") <*> (reserved "BY" *> lexeme parseExpr) <*> (Desc <$ reserved "DESC")
    orderAsc = (OrderBy <$ reserved "ORDER") <*> (reserved "BY" *> lexeme parseExpr) <*> (Asc <$ optional (reserved "ASC"))

-- Parses a value with an optional typecast
parseValue :: Bool -> Parser Value
parseValue useFullCond = typecasted <|> parseInnerValue useFullCond where
    typecasted = try $ Type <$> parseInnerValue useFullCond <*> (symbol "::" *> parseWord)

-- Parses the Value data type
parseInnerValue :: Bool -> Parser Value
parseInnerValue useFullCond
    | useFullCond = cas <|> txt <|> fullCond <|> expr <|> comm <|> tup
    | otherwise = cas <|> txt <|> singCond <|> expr <|> comm <|> tup where
    cas = (Case <$ reserved "CASE") <*> many1 whenThen <*> optionMaybe (reserved "ELSE" *> parseValue True) <* reserved "END"
    txt = Text <$> lexeme (between (char '\'') (char '\'') (many $ noneOf "\'"))
    fullCond = try $ Cond <$> lexeme parseOr -- complete condition type
    singCond = try $ Cond <$> lexeme parseSingleCond -- only TRUE, FALSE or a complete condition in parentheses
    expr = try $ Exp <$> lexeme parseExpr
    comm = try $ Comm <$> parens parseCommand
    tup = parseTuple

-- Parses a Tuple of values
parseTuple :: Parser Value
parseTuple = Tuple <$> parens (sepBy1 (parseValue True) (symbol ","))

-- Parses "WHEN condition THEN value"
whenThen :: Parser (Condition, Value)
whenThen = (,) <$> (reserved "WHEN" *> parseOr) <*> (reserved "THEN" *> parseValue True)

-- Parses the Expr(ession) data type: bitwise operations
parseExpr :: Parser Expr
parseExpr = calc <|> child where
    calc = try $ Calc <$> child <*> many1 ((,) <$> op <*> child)
    child = parseArithExpr
    op = parseOperator [BAnd, BOr]

-- Parses the Expr(ession) data type: arithmetic (+,-,%) operations
parseArithExpr :: Parser Expr
parseArithExpr = calc <|> child where
    calc = try $ Calc <$> child <*> many1 ((,) <$> op <*> child)
    child = parseTerm
    op = parseOperator [Add, Subtract, Modulo]

-- Parses the Expr(ession) data type: multiply/divide operations
parseTerm :: Parser Expr
parseTerm = calc <|> child where
    calc = try $ Calc <$> child <*> many1 ((,) <$> op <*> child)
    child = parsePower
    op = parseOperator [Multiply, Divide]

-- Parses the Expr(ession) data type: a^b operations
parsePower :: Parser Expr
parsePower = calc <|> child where
    calc = try $ Calc <$> child <*> many1 ((,) <$> op <*> child)
    child = parseFactor
    op = parseOperator [Power]

-- Parses the Expr(ession) data type: factors
parseFactor :: Parser Expr
parseFactor = num <|> fun <|> col <|> par where
    num = Num <$> parseNumber
    fun = try $ FunCall <$> parseWord <*> parens (sepBy (parseValue True) (symbol ","))
    col = ColCall <$> parseName
    par = parens $ parseExpr

-- Parses the Condition data type: OR operations
parseOr :: Parser Condition
parseOr = cond <|> child where
    cond = try $ Or <$> child <*> many1 (op *> child)
    child = parseAnd
    op = reserved "OR"

-- Parses the Condition data type: AND operations
parseAnd :: Parser Condition
parseAnd = cond <|> child where
    cond = try $ And <$> child <*> many1 (op *> child)
    child = parseCondition
    op = reserved "AND"

-- Parses the Condition data type: factors
parseCondition :: Parser Condition
parseCondition = not <|> compin <|> comp <|> between <|> single where
    not = Not <$> (reserved "NOT" *> parseCondition)
    compin = try $ Compare <$> parseValue False <*> parseOperator [In] <*> parseTuple -- value IN (val1, val2, val3)
    comp = try $ Compare <$> parseValue False <*> op <*> parseValue False
    between = try $ Between <$> (parseExpr <* reserved "BETWEEN") <*> (parseExpr <* reserved "AND") <*> parseExpr -- age BETWEEN 18 AND 65
    single = parseSingleCond
    op = parseOperator [Equal, GreatEq, LessEq, NotEq, Greater, Less, Like, In]

-- Parses the Condition: TRUE, FALSE or complete condition in parantheses
-- Used since conditions in comparison can't be another comparison without parentheses
-- TRUE = TRUE = TRUE is not allowed, (TRUE = TRUE) = TRUE, is
parseSingleCond :: Parser Condition
parseSingleCond = par <|> true <|> false where
    par = parens parseOr
    true = IsTrue <$ reserved "TRUE"
    false = IsFalse <$ reserved "FALSE"

-- Creates a parser for one or more given operators
parseOperator :: [Operator] -> Parser Operator
parseOperator [op] = try $ op <$ stringsToParser (getIcons op)
parseOperator (op:xs) = parseOperator [op] <|> parseOperator xs

--- Function that uses a given parser to parse a given string
run :: Parser a -> String -> a
run p xs = case res of
    Left x -> error $ show $ x
    Right x -> x
    where res = parse p "" xs

{-
Helper (parser) functions for the parser above
-}

-- Parses a word
parseWord :: Parser String
parseWord = par <|> nopar where
    par = lexeme $ between (char '\"') (char '\"') (many1 $ noneOf "\"") -- any character (including spaces) between double quotes
    nopar = identifier -- single word, no spaces or some other used characters

-- Parses a given character (both upper and lower case)
parseLetter :: Char -> Parser Char
parseLetter c = upper <|> lower where
    upper = char $ toUpper c
    lower = char $ toLower c

-- Parses a given string (uses parseLetter)
parseString :: String -> Parser String
parseString [] = pure []
parseString [c] = (:) <$> (parseLetter c <* ws) <*> pure [] where
    ws = char ' ' <|> char '\n' <|> char '\t'
parseString (c:xs) = (:) <$> parseLetter c <*> parseString xs

-- Creates a parser that can parse multiple strings
stringsToParser :: [String] -> Parser String
stringsToParser [str]
    | isLetter (str!!0) = lexeme $ parseString str -- uses parseString if str is a word
    | otherwise = symbol str -- and symbol if str is a symbol (=, >, etc.)
stringsToParser (str:xs) = stringsToParser [str] <|> stringsToParser xs

-- Parses a number as a string
parseNumber :: Parser String
parseNumber = ((:) <$> char '-' <*> restNumber) <|> restNumber

restNumber :: Parser String
restNumber = lexeme $ many1 (digit <|> char '.')