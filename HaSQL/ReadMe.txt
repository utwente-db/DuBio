1. Install Haskell together with GHC and Cabal-Install (see haskell.org)
2. Run the following commands in your terminal
>> cabal update
>> cabal install parsec
>> cabal install postgresql-simple
(This last one could fail, because postgresql needs to be added
to the environment variables (atleast on windows)
see https://github.com/lpsmith/postgresql-simple/pull/66 for a fix on windows)
3. Run GHCi in this folder
4. Run the following commands in GHCi
>> :l Main
>> main
5. You should be able to connect to a database now
6. Enter your inSQeLto commands