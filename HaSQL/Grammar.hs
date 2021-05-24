module Grammar where

data Command
    = Select [Col] [Refine]
    | None
    deriving (Show, Eq)

data Col
    = AllOf Name
    | Col Value
    | ColAs Value String
    deriving (Show, Eq)

data TabCont -- Table Container
    = Tabs Table [(Join, Table, Condition)]
    deriving (Show, Eq)

data Table
    = Table Name
    | TableAs Name String
    deriving (Show, Eq)

data Join
    = Inner
    | Lft
    | Rght
    | Full
    deriving (Show, Eq)

data Refine
    = From [TabCont]
    | Where Condition
    | GroupBy Name
    | OrderBy Expr Order
    | Limit String
    deriving (Show, Eq)
-- When adding a new refine element also update
-- the refMap function with the right order

data Order
    = Asc
    | Desc
    deriving (Show, Eq)

data Name
    = Name [String]
    deriving (Show, Eq)

data Condition
    = Compare Value Operator Value
    | Between Value Value Value
    | And Condition [Condition]
    | Or Condition [Condition]
    | Not Condition
    | IsTrue
    | IsFalse
    deriving (Show, Eq)

data Value
    = Exp Expr
    | Tuple [Value]
    | Cond Condition
    | Text String
    | Case [(Condition, Value)] (Maybe Value)
    | Comm Command -- can only be a select command
    deriving (Show, Eq)

data Expr
    = Calc Expr [(Operator, Expr)]
    | ColCall Name
    | FunCall String [Value]
    | Num String
    deriving (Show, Eq)
    
data Operator
    -- comparison
    = Equal
    | Greater
    | Less
    | GreatEq
    | LessEq
    | NotEq
    | Like
    | In
    -- arithmetic
    | Add
    | Subtract
    | Modulo
    | Multiply
    | Divide
    | Power
    -- bitwise
    | BAnd
    | BOr
    -- condition
    | CAnd
    | COr
    | CNot
    -- zero
    | Zero
    deriving (Show, Eq)

-- Used to order the list of refine elements
refMap :: Refine -> Int
refMap (From _) = 0
refMap (Where _) = 1
refMap (GroupBy _) = 2
refMap (OrderBy _ _) = 3
refMap (Limit _) = 4

operatorMap :: Operator -> (Int, [String])
operatorMap Zero = (0, [])
operatorMap COr = (1, ["OR"])
operatorMap CAnd = (2, ["AND"])
operatorMap CNot = (3, ["NOT"])
operatorMap Equal = (4, ["="])
operatorMap Greater = (4, [">"])
operatorMap Less = (4, ["<"])
operatorMap GreatEq = (4, [">="])
operatorMap LessEq = (4, ["<="])
operatorMap NotEq = (4, ["<>", "!="])
operatorMap Like = (4, ["LIKE"])
operatorMap In = (4, ["IN"])
operatorMap BAnd = (5, ["&"])
operatorMap BOr = (5, ["|"])
operatorMap Add = (6, ["+"])
operatorMap Subtract = (6, ["-"])
operatorMap Modulo = (6, ["%"])
operatorMap Multiply = (7, ["*"])
operatorMap Divide = (7, ["/"])
operatorMap Power = (8, ["^"])

getIcons :: Operator -> [String]
getIcons op = (\(_, strs) -> strs) $ operatorMap op

getIcon :: Operator -> String
getIcon op = (getIcons op)!!0

getLevel :: Operator -> Int
getLevel op = (\(l, _) -> l) $ operatorMap op