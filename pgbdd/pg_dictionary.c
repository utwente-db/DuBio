/* 
 * This file is part of the Dubio distribution (https://github.com/utwente-db/DuBio).
 * Copyright (c) 2020 Jan Flokstra & Maurice van Keulen
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "pg_config.h"

#include "dictionary.c"

PG_FUNCTION_INFO_V1(dictionary_in);
/**
 * <code>dictionary_in(vardef cstring) returns dictionary</code>
 * Create a dictionary initialised from a text expression containg vardefs.
 *
 */
Datum
dictionary_in(PG_FUNCTION_ARGS)
{       
    bdd_dictionary new_dict_struct, *new_dict;

    char *dictname = PG_GETARG_CSTRING(0);
    if ( !(new_dict = bdd_dictionary_create(&new_dict_struct,dictname)) )
        ereport(ERROR,(errmsg("dictionary_in: dictionary create \'%s\' failed",dictname)));
    bdd_dictionary* return_dict = bdd_dictionary_serialize(new_dict);
    bdd_dictionary_free(new_dict);
    bdd_dictionary_sort(return_dict); // INCOMPLETE ???
    SET_VARSIZE(return_dict,return_dict->size);
    PG_RETURN_DICTIONARY(return_dict);
}


PG_FUNCTION_INFO_V1(dictionary_out);
/**
 * <code>dictionary_out(dictionary dictionary) returns text</code>
 * Create a text representation of a dictionary expression.
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_out(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict = PG_GETARG_DICTIONARY(0);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bprintf(pbuff,"[Dictionary(name=%s, #vars=%d, #values=%d)]",dict->name,V_dict_var_size(dict->variables),V_dict_val_size(dict->values)-dict->val_deleted);
    char* result = pbuff_preserve_or_alloc(pbuff);
    PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(dictionary_print);
/**
 * <code>dictionary_print(dictionary dictionary) returns text</code>
 * Create a text representation of a dictionary expression.
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_print(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict = PG_GETARG_DICTIONARY(0);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_dictionary_print(dict,0/*all*/,pbuff);
    char* result = pbuff_preserve_or_alloc(pbuff);
    PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(dictionary_add);
/**
 * <code>dictionary_add(dictionary dictionary) returns new dictionary with addes vars</code>
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_add(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict    = PG_GETARG_DICTIONARY(0);
    char            *vardefs = PG_GETARG_CSTRING(1);
    bdd_dictionary  *return_dict = NULL;
    char            *_errmsg     = NULL;

    if ( !dict )
        ereport(ERROR,(errmsg("dictionary_add: %s","internal error bad dict parameter")));
    if ( !modify_dictionary(dict,DICT_ADD,vardefs,&_errmsg) )
        ereport(ERROR,(errmsg("%s",_errmsg)));
    if ( !(
           (return_dict = bdd_dictionary_serialize(dict))  &&
           bdd_dictionary_free(dict) &&
           bdd_dictionary_sort(return_dict) ))
        ereport(ERROR,(errmsg("dictionary_add: %s","internal error serialize/free/sort")));
    SET_VARSIZE(return_dict,return_dict->size);
    PG_RETURN_DICTIONARY(return_dict);
}


PG_FUNCTION_INFO_V1(dictionary_del);
/**
 * <code>dictionary_del(dictionary dictionary) returns new dictionary with addes vars</code>
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_del(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict    = PG_GETARG_DICTIONARY(0);
    char            *vardefs = PG_GETARG_CSTRING(1);
    bdd_dictionary  *return_dict = NULL;
    char            *_errmsg     = NULL;

    if ( !dict )
        ereport(ERROR,(errmsg("dictionary_del: %s","internal error bad dict parameter")));
    if ( !modify_dictionary(dict,DICT_DEL,vardefs,&_errmsg) )
        ereport(ERROR,(errmsg("%s",_errmsg)));
    if ( !(
           (return_dict = bdd_dictionary_serialize(dict))  &&
           bdd_dictionary_free(dict) &&
           bdd_dictionary_sort(return_dict) ))
        ereport(ERROR,(errmsg("dictionary_del: %s","internal error serialize/free/sort")));
    SET_VARSIZE(return_dict,return_dict->size);
    PG_RETURN_DICTIONARY(return_dict);
}


PG_FUNCTION_INFO_V1(dictionary_upd);
/**
 * <code>dictionary_upd(dictionary dictionary) returns new dictionary with addes vars</code>
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_upd(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict    = PG_GETARG_DICTIONARY(0);
    char            *vardefs = PG_GETARG_CSTRING(1);
    bdd_dictionary  *return_dict = NULL;
    char            *_errmsg     = NULL;

    if ( !dict )
        ereport(ERROR,(errmsg("dictionary_upd: %s","internal error bad dict parameter")));
    if ( !modify_dictionary(dict,DICT_UPD,vardefs,&_errmsg) )
        ereport(ERROR,(errmsg("%s",_errmsg)));
    if ( !(
           (return_dict = bdd_dictionary_serialize(dict))  &&
           bdd_dictionary_free(dict) &&
           bdd_dictionary_sort(return_dict) ))
        ereport(ERROR,(errmsg("dictionary_upd: %s","internal error serialize/free/sort")));
    SET_VARSIZE(return_dict,return_dict->size);
    PG_RETURN_DICTIONARY(return_dict);
}


