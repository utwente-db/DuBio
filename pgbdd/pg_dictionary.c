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
    bdd_dictionary new_dict_struct, *dict;
    bdd_dictionary  *storage_dict = NULL;
    char            *_errmsg      = NULL;

    char *vardefs = PG_GETARG_CSTRING(0);
    if ( !(dict = bdd_dictionary_create(&new_dict_struct)) )
        ereport(ERROR,(errmsg("dictionary_in: dictionary create' failed")));
    if ( !modify_dictionary(dict,DICT_ADD,vardefs,&_errmsg) )
        ereport(ERROR,(errmsg("%s",(_errmsg ? _errmsg : "NULL"))));
    if ( !(storage_dict = dictionary_prepare2store(dict)) )
        ereport(ERROR,(errmsg("dictionary_add: %s","internal error serialize/free/sort")));
    SET_VARSIZE(storage_dict,storage_dict->bytesize);
    PG_RETURN_DICTIONARY(storage_dict);
}


PG_FUNCTION_INFO_V1(dictionary_out);
/**
 * <code>dictionary_out(dictionary dictionary) returns cstring</code>
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
    bprintf(pbuff,"[Dictionary(#vars=%d, #values=%d)]",V_dict_var_size(dict->variables),V_dict_val_size(dict->values)-dict->val_deleted);
    char* result = pbuff2cstring(pbuff,-1);
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
    text* result = pbuff2text(pbuff,-1);
    PG_RETURN_TEXT_P(result);
}

PG_FUNCTION_INFO_V1(dictionary_debug);
/**
 * <code>dictionary_debug(dictionary dictionary) returns text</code>
 * Create a text representation of the dictionary internals
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_debug(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict = PG_GETARG_DICTIONARY(0);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_dictionary_print(dict,1/*all*/,pbuff);
    text* result = pbuff2text(pbuff,-1);
    PG_RETURN_TEXT_P(result);
}

PG_FUNCTION_INFO_V1(dictionary_merge);
/**
 * <code>dictionary_merge(ldict dictionary, rdict dictionary) returns new merged dictionary</code>
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_merge(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *ldict        = PG_GETARG_DICTIONARY(0);
    bdd_dictionary  *rdict        = PG_GETARG_DICTIONARY(1);
    bdd_dictionary   s_merged_dict;
    bdd_dictionary  *merged_dict  = NULL;
    bdd_dictionary  *storage_dict = NULL;
    char            *_errmsg      = NULL;

    if ( !ldict )
        ereport(ERROR,(errmsg("dictionary_merge: %s","internal error bad dict parameter")));
    if ( !(merged_dict=merge_dictionary(&s_merged_dict,ldict,rdict,&_errmsg)) )
        ereport(ERROR,(errmsg("%s",(_errmsg ? _errmsg : "NULL"))));
    if ( !(storage_dict = dictionary_prepare2store(merged_dict)) )
        ereport(ERROR,(errmsg("dictionary_merge: %s","internal error serialize/free/sort")));
    SET_VARSIZE(storage_dict,storage_dict->bytesize);
    PG_RETURN_DICTIONARY(storage_dict);
}

PG_FUNCTION_INFO_V1(dictionary_modify);
/**
 * <code>dictionary_modify(dictionary dictionary) returns new dictionary with addes vars</code>
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr dictionary</code> A dictionary object
 * @return <code>cstring</code> the string representation of the dictionary
 */
Datum
dictionary_modify(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict         = PG_GETARG_DICTIONARY(0);
    char            mode          = PG_GETARG_INT32(1);
    char            *vardefs      = PG_GETARG_CSTRING(2);
    bdd_dictionary  *storage_dict = NULL;
    char            *_errmsg      = NULL;

    if ( !dict )
        ereport(ERROR,(errmsg("dictionary_modify: %s","internal error bad dict parameter")));
    if ( !modify_dictionary(dict,mode,vardefs,&_errmsg) )
        ereport(ERROR,(errmsg("%s",(_errmsg ? _errmsg : "NULL"))));
    if ( !(storage_dict = dictionary_prepare2store(dict)) )
        ereport(ERROR,(errmsg("dictionary_add: %s","internal error serialize/free/sort")));
    SET_VARSIZE(storage_dict,storage_dict->bytesize);
    PG_RETURN_DICTIONARY(storage_dict);
}
