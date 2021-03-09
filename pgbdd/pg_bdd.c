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

    if ( !(return_bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,0/*verbose*/)) )
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
    char* result = bdd2cstring(par_bdd,1/*encapsulation*/);
    PG_RETURN_CSTRING(result);
}

PG_FUNCTION_INFO_V1(bdd_pg_operator);
/**
 * <code>bdd_in(expression cstring) returns bdd</code>
 * Create an expression from argument string.
 *
 */
Datum
bdd_pg_operator(PG_FUNCTION_ARGS)
{       
    char *operator  = PG_GETARG_CSTRING(0);
    bdd *lhs_bdd    = PG_GETARG_BDD(1);
    bdd *rhs_bdd    = NULL;
    
    char *_errmsg    = NULL;
    bdd  *return_bdd = NULL;

    if ( *operator == '&' || *operator == '|' )
        rhs_bdd    = PG_GETARG_BDD(2);
    if ( !(return_bdd = bdd_operator(*operator,BY_APPLY,lhs_bdd,rhs_bdd,&_errmsg)))
    // if ( !(return_bdd = bdd_operator(*operator,BY_TEXT,lhs_bdd,rhs_bdd,&_errmsg)))
        ereport(ERROR,(errmsg("bdd_operator: error: %s ",(_errmsg ? _errmsg : "NULL"))));
    SET_VARSIZE(return_bdd,return_bdd->bytesize);
    PG_RETURN_BDD(return_bdd);
}

PG_FUNCTION_INFO_V1(alg_bdd);
/**
 * <code>bdd_in(expression cstring) returns bdd</code>
 * Create an expression from argument string.
 *
 */
Datum
alg_bdd(PG_FUNCTION_ARGS)
{       
    char *alg_name   = PG_GETARG_CSTRING(0);
    bdd_alg *alg     = NULL;
    char *expr       = PG_GETARG_CSTRING(1);
    char *_errmsg    = NULL;
    bdd  *return_bdd = NULL;

    
    if ( !(alg = bdd_algorithm(alg_name,&_errmsg)) )
        ereport(ERROR,(errmsg("bdd_in: %s",(_errmsg ? _errmsg : "NULL"))));
    if ( !(return_bdd = create_bdd(alg,expr,&_errmsg,0/*verbose*/)) )
        ereport(ERROR,(errmsg("bdd_in: %s",(_errmsg ? _errmsg : "NULL"))));
    SET_VARSIZE(return_bdd,return_bdd->bytesize);
    PG_RETURN_BDD(return_bdd);
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
    bdd *par_bdd = PG_GETARG_BDD(0);
    text* result = bdd2text(par_bdd,0/*print bdd() encapsulation*/);
    PG_RETURN_TEXT_P(result);
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

    text* result;
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_info(par_bdd, pbuff);
    result = pbuff2text(pbuff,-1);
    PG_RETURN_TEXT_P(result);
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

    text* result;
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_generate_dot(par_bdd,pbuff,NULL);

    if ( dotfile && strlen(dotfile) > 0) {
        FILE* f = fopen(dotfile,"w");

        if ( f ) {
            fputs(pbuff->buffer,f);
            fclose(f);
        }
    }
    result = pbuff2text(pbuff,-1);
    PG_RETURN_TEXT_P(result);
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
        ereport(ERROR,(errmsg("bdd_pg_prob: %s",(_errmsg ? _errmsg : "NULL"))));
    PG_RETURN_FLOAT8(prob);
    // PG_RETURN_NUMERIC(prob); CRASHES SERVER
}

PG_FUNCTION_INFO_V1(bdd_pg_prob_by_ref);
/**
 * <code>bdd_pg_prob(dict dictionary, bdd bdd) returns double</code>
 * Computes probability of bdd expression with defined rva probs in dictionary
 * This prob uses the cached dictionary_ref as dictionary to speed up 
 *
 */
Datum
bdd_pg_prob_by_ref(PG_FUNCTION_ARGS)
{
    bdd_dictionary_ref  *bdr = PG_GETARG_DICTIONARY_REF(0);
    bdd_dictionary *dict     = NULL;
    bdd             *par_bdd = PG_GETARG_BDD(1);

    char* _errmsg;
    if ( !(dict=get_dict_from_ref(bdr,&_errmsg) ))
        ereport(ERROR,(errmsg("bdd_pg_prob_by_ref: %s",(_errmsg ? _errmsg : "NULL"))));
    double prob = bdd_probability(dict,par_bdd,NULL,0,&_errmsg);
    if ( prob < 0.0 )
        ereport(ERROR,(errmsg("bdd_pg_prob: %s",(_errmsg ? _errmsg : "NULL"))));
    PG_RETURN_FLOAT8(prob);
}

PG_FUNCTION_INFO_V1(pg_bdd_contains);
/**
 * <code>bdd_contains(bdd bdd, var cstring, val int) returns boolean</code>
 * Check if a bdd contains rva (var=val), val = -1 means all vars
 *
 */
Datum
pg_bdd_contains(PG_FUNCTION_ARGS)
{       
    bdd  *par_bdd     = PG_GETARG_BDD(0);
    char *var         = PG_GETARG_CSTRING(1);
    int   val         = PG_GETARG_INT32(2);
    int   res         = -1;
    char *_errmsg     = NULL;

    if ( (res = bdd_contains(par_bdd,var,val,&_errmsg)) < 0 )
        ereport(ERROR,(errmsg("bdd_contains: %s=%d: %s",var,val,(_errmsg ? _errmsg : "NULL"))));
    PG_RETURN_BOOL(res);
}

PG_FUNCTION_INFO_V1(pg_bdd_restrict);
/**
 * <code>bdd_restrict(bdd bdd, var cstring, val integer, torf boolean) returns bdd</code>
 * Restrict the value of an rva (var = val) to a boolean value (torf). val = -1 means var=*
 *
 */
Datum
pg_bdd_restrict(PG_FUNCTION_ARGS)
{       
    bdd  *par_bdd     = PG_GETARG_BDD(0);
    char *var         = PG_GETARG_CSTRING(1);
    int   val         = PG_GETARG_INT32(2);
    int   torf        = PG_GETARG_BOOL(3);
    bdd  *return_bdd  = NULL;
    char *_errmsg     = NULL;

    if ( !(return_bdd = bdd_restrict(par_bdd,var,val,torf,0,&_errmsg)) )
        ereport(ERROR,(errmsg("bdd_restrict: %s=%d/%s: %s",var,val,(torf?"TRUE":"FALSE"),(_errmsg ? _errmsg : "NULL"))));
    SET_VARSIZE(return_bdd,return_bdd->bytesize);
    PG_RETURN_BDD(return_bdd);
}

PG_FUNCTION_INFO_V1(bdd_has_property);
/**
 * <code>bdd_has_property(bdd bdd, mode integer, s cstring) returns boolean</code>
 * Check a property of a bdd
 *
 */
Datum
bdd_has_property(PG_FUNCTION_ARGS)
{       
    bdd  *par_bdd     = PG_GETARG_BDD(0);
    int   property    = PG_GETARG_INT32(1);
    char *s           = PG_GETARG_CSTRING(2);
    int   res         = -1;
    char *_errmsg     = NULL;

    if ( (res = bdd_property_check(par_bdd,property,s,&_errmsg)) < 0 )
        ereport(ERROR,(errmsg("bdd_has_property: %d %s",property,(_errmsg ? _errmsg : "NULL"))));
    PG_RETURN_BOOL(res);
}
