module Parser where

import Text.ParserCombinators.Parsec
import Text.ParserCombinators.Parsec.Language
import qualified Text.ParserCombinators.Parsec.Token as Token

import Grammar

import Data.Char

languageDef =
    emptyDef { Token.identStart      = noneOf " \t\n.,\"();*+-=<>!&|^%" 
             , Token.identLetter     = noneOf " \t\n.,\"();*+-=<>!&|^%"             
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
integer = Token.integer lexer
parens = Token.parens lexer
braces = Token.braces lexer
brackets = Token.brackets lexer
commaSep = Token.commaSep lexer
semiSep = Token.semiSep lexer
symbol = Token.symbol lexer
reserved = Token.reserved lexer
whitespace = Token.whiteSpace lexer



--- Helper functions for the parser

-- Parses a word
parseWord :: Parser String
parseWord = par <|> nopar where
    par = try $ lexeme $ between (char '\"') (char '\"') (many1 $ noneOf "\"") -- any character (including spaces) between double quotes
    nopar = try $ identifier -- single word, no spaces or some other used characters

-- Parses a given character (both upper and lower case)
parseLetter :: Char -> Parser Char
parseLetter c = upper <|> lower where
    upper = char $ toUpper c
    lower = char $ toLower c

-- Parses a given string (uses parseLetter)
parseString :: String -> Parser String
parseString [] = pure []
parseString (c:xs) = (:) <$> parseLetter c <*> parseString xs

-- Creates a parser that can parse multiple strings
stringsToParser :: [String] -> Parser String
stringsToParser [str]
    | isLetter (str!!0) = lexeme $ parseString str -- uses parseString if str is a word
    | otherwise = symbol str -- and symbol if str is a symbol (=, >, etc.)
stringsToParser (str:xs) = stringsToParser [str] <|> stringsToParser xs

-- Parses a number as a string
parseNumber :: Parser String
parseNumber = lexeme $ many1 (digit <|> char '.')

--- Functions that parse the grammar from Grammar.hs

-- Parses a sequence of commands
parseProgram :: Parser [Command]
parseProgram = sepBy1 parseCommand (symbol ";") <* eof

-- Parses the Command data type
parseCommand :: Parser Command
parseCommand = select <|> none where
    select = try $ (Select <$ reserved "SELECT") <*> sepBy1 parseCol (symbol ",") <*> many parseRefine
    none = try $ None <$ lexeme whitespace -- empty command

-- Parses the Col(umn) data type
parseCol :: Parser Col
parseCol = colAs <|> col <|> allOf where
    colAs = try $ ColAs <$> parseValue True <*> ((optional $ reserved "AS") *> parseWord)
    col = try $ Col <$> parseValue True
    allOf = try $ AllOf <$> parseAllOf

-- Parses the TabCont (Table Container) data type
parseTabCont :: Parser TabCont
parseTabCont = Tabs <$> parseTable <*> many ((,,) <$> parseJoin <*> parseTable <*> (reserved "ON" *> parseCondition))

-- Parser the Join data type
parseJoin :: Parser Join
parseJoin = join <|> left <|> right <|> full where
    join = try $ Inner <$ (optional $ reserved "INNER") <* reserved "JOIN"
    left = try $ Lft <$ reserved "LEFT" <* (optional $ reserved "OUTER") <* reserved "JOIN"
    right = try $ Rght <$ reserved "RIGHT" <* (optional $ reserved "OUTER") <* reserved "JOIN"
    full = Full <$ reserved "FULL" <* (optional $ reserved "OUTER") <* reserved "JOIN"

-- Parses the Table data type
parseTable :: Parser Table
parseTable = tableAs <|> table where
    tableAs = try $ TableAs <$> parseName <*> ((optional $ reserved "AS") *> parseWord)
    table = try $ Table <$> parseName

-- Parser the Name data type
parseName :: Parser Name
parseName = try $ Name <$> sepBy1 parseWord (symbol ".")

-- Same as parseName but can also parse * between the dots
parseAllOf :: Parser Name
parseAllOf = try $ Name <$> sepBy1 (word <|> star) (symbol ".") where
    word = try $ parseWord
    star = try $ symbol "*"

-- Parses the Refine data type
parseRefine :: Parser Refine
parseRefine = from <|> whr <|> group <|> orderAsc <|> orderDesc <|> order <|> limit where
    from = try $ (From <$ reserved "FROM") <*> sepBy1 parseTabCont (symbol",")
    whr = try $ (Where <$ reserved "WHERE") <*> parseOr
    group = try $ (GroupBy <$ reserved "GROUP") <*> (reserved "BY" *> lexeme parseName)
    orderAsc = try $ (OrderBy <$ reserved "ORDER") <*> (reserved "BY" *> lexeme parseExpr) <*> (Asc <$ reserved "ASC")
    orderDesc = try $ (OrderBy <$ reserved "ORDER") <*> (reserved "BY" *> lexeme parseExpr) <*> (Desc <$ reserved "DESC")
    order = try $ (OrderBy <$ reserved "ORDER") <*> (reserved "BY" *> lexeme parseExpr) <*> (Asc <$ symbol "")
    limit = try $ (Limit <$ reserved "LIMIT") <*> lexeme (many1 (oneOf "0123456789"))

-- Parses the Value data type
parseValue :: Bool -> Parser Value
parseValue useFullCond
    | useFullCond = cas <|> txt <|> fullCond <|> expr <|> comm <|> tup
    | otherwise = cas <|> txt <|> singCond <|> expr <|> comm <|> tup where
    cas = try $ (Case <$ reserved "CASE") <*> many1 whenThen <*> optionMaybe (reserved "ELSE" *> parseValue True) <* reserved "END"
    txt = try $ Text <$> lexeme (between (char '\'') (char '\'') (many $ noneOf "\'"))
    fullCond = try $ Cond <$> lexeme parseOr -- complete condition type
    singCond = try $ Cond <$> lexeme parseSingleCond -- only TRUE, FALSE or a complete condition in parentheses
    expr = try $ Exp <$> lexeme parseExpr
    comm = try $ Comm <$> parens parseCommand
    tup = Tuple <$> parens (sepBy1 (parseValue True) (symbol ","))

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
parseFactor = par <|> num <|> fun <|> col where
    par = parens $ parseExpr
    num = try $ Num <$> parseNumber
    fun = try $ FunCall <$> parseWord <*> parens (sepBy (parseValue True) (symbol ","))
    col = try $ ColCall <$> parseName

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
parseCondition = not <|> comp <|> between <|> single where
    not = try $ Not <$> (reserved "NOT" *> parseCondition)
    comp = try $ Compare <$> parseValue False <*> op <*> parseValue False
    between = try $ Between <$> (parseValue False <* reserved "BETWEEN") <*> (parseValue False <* reserved "AND") <*> parseValue False
    single = try $ parseSingleCond
    op = parseOperator [Equal, GreatEq, LessEq, NotEq, Greater, Less, Like, In]

-- Parses the Condition: TRUE, FALSE or complete condition in parantheses
parseSingleCond :: Parser Condition
parseSingleCond = par <|> true <|> false where
    par = try $ parens parseOr
    true = try $ IsTrue <$ reserved "TRUE"
    false = try $ IsFalse <$ reserved "FALSE"

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