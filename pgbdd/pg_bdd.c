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

PG_MODULE_MAGIC;

#include "bdd.c"

PG_FUNCTION_INFO_V1(bdd_in);
/**
 * <code>bdd_in(expression cstring) returns bdd</code>
 * Create an expression from argument string.
 *
 */
Datum
bdd_in(PG_FUNCTION_ARGS)
{       
    char *expr       = PG_GETARG_CSTRING(0);
    char *_errmsg    = NULL;
    bdd  *return_bdd = NULL;

    if ( !(return_bdd = create_bdd(BDD_BASE,expr,&_errmsg,0/*verbose*/)) )
        ereport(ERROR,(errmsg("bdd_in: %s",(_errmsg ? _errmsg : "NULL"))));
    SET_VARSIZE(return_bdd,return_bdd->bytesize);
    PG_RETURN_BDD(return_bdd);
}


PG_FUNCTION_INFO_V1(bdd_out);
/**
 * <code>bdd_out(bdd bdd) returns text</code>
 * Create a text representation of a bdd expression.
 *
 */
Datum
bdd_out(PG_FUNCTION_ARGS)
{
    bdd *par_bdd = PG_GETARG_BDD(0);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bprintf(pbuff,"Bdd(\'%s\')",par_bdd->expr);
    char* result = pbuff_preserve_or_alloc(pbuff);
    PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(bdd_pg_tostring);
/**
 * <code>bdd_pg_tostring(bdd bdd) returns text</code>
 * Create tostring representation of a bdd expression.
 *
 */
Datum
bdd_pg_tostring(PG_FUNCTION_ARGS)
{
    bdd* par_bdd  = PG_GETARG_BDD(0);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bprintf(pbuff,"%s",par_bdd->expr);
    char* result = pbuff_preserve_or_alloc(pbuff);
    PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(bdd_pg_info);
/**
 * <code>bdd_pg_info(bdd bdd) returns text</code>
 * Create info representation of a bdd expression.
 *
 */
Datum
bdd_pg_info(PG_FUNCTION_ARGS)
{
    bdd* par_bdd  = PG_GETARG_BDD(0);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_info(par_bdd, pbuff);
    char* result = pbuff_preserve_or_alloc(pbuff);
    PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(bdd_pg_dot);
/**
 * <code>bdd_pg_dot(bdd bdd) returns text</code>
 * Create Graphviz DOT representation of a bdd expression.
 *
 */
Datum
bdd_pg_dot(PG_FUNCTION_ARGS)
{
    bdd*  par_bdd  = PG_GETARG_BDD(0);
    char* dotfile  = PG_GETARG_CSTRING(1);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_generate_dot(par_bdd,pbuff,NULL);
    char* result = pbuff_preserve_or_alloc(pbuff);

    if ( dotfile && strlen(dotfile) > 0) {
        FILE* f = fopen(dotfile,"w");

        if ( f ) {
            fputs(result,f);
            fclose(f);
        }
    }
    PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(bdd_pg_prob);
/**
 * <code>bdd_pg_prob(dict dictionary, bdd bdd) returns double</code>
 * Computes probability of bdd expression with defined rva probs in dictionary
 *
 */
Datum
bdd_pg_prob(PG_FUNCTION_ARGS)
{
    bdd_dictionary  *dict     = PG_GETARG_DICTIONARY(0);
    bdd             *par_bdd  = PG_GETARG_BDD(1);

    char* _errmsg;
    double prob = bdd_probability(dict,par_bdd,NULL,0,&_errmsg);
    if ( prob < 0.0 )
        ereport(ERROR,(errmsg("bdd_pg_prob: %s",_errmsg)));
    PG_RETURN_FLOAT8(prob);
}


