module Names where
{-
Some names that are used in Converter.hs
that are used to generate the new query
-}

-- Table name of the dictionary
dictTableName :: String
dictTableName = "_dict"

-- Column of dictionary table that stores the name of a dictionary
dictNameCol :: String
dictNameCol = "name"

-- Column of dictionary table that stores the dictionary
dictDictCol :: String
dictDictCol = "dict"

-- Name of the dictionary that is called
dictName :: String
dictName = "mydict"

-- Name of the column that stores the sentence in a probabilistic table
sentenceCol :: String
sentenceCol = "_sentence"

-- Name of the attribute that is called as a column to get the probability
probAttr :: String
probAttr = "_prob"

-- What the probability column is called in the results
probName :: String
probName = "probability"