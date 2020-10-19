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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"
#include "bdd.h"
#include "utils.h"
#include "dictionary.h"

//

DefVectorC(bddstr);

int cmpBddstr(bddstr* l, bddstr* r) {  return strcmp(l->str,r->str); }

//

DefVectorC(bddrow);

int cmpBddrow(bddrow* l, bddrow* r) {
    int res = strcmp(l->rva, r->rva);
    if (res == 0) {
        res = l->low - r->low;
        if ( res == 0 )
            res = l->high - r->high;
    }
    return res;
} 

/*
 *
 *
 */

bdd_runtime* bdd_init(bdd_runtime* bdd, char* expr, V_bddstr* order, int verbose) {
    fprintf(stdout,"Create bdd\n");
    bdd->verbose = verbose;
    V_bddrow_init(&bdd->core.tree);
    //
    bdd->core.expr  = expr;
    bdd->order = (order ? *order :bdd_set_default_order(bdd->core.expr));
    bdd->n     = V_bddstr_size(&bdd->order);
    bdd->mk_calls = 0;
    //
    return bdd;
}

void bdd_free(bdd_runtime* bdd) {
    V_bddstr_free(&bdd->order);
    V_bddrow_free(&bdd->core.tree);
}

static void bdd_print(bdd_runtime* bdd, FILE* out) {
    fprintf(out,"BDD: \n");
    fprintf(out,"+ expr:\t%s\n",bdd->core.expr);
    fprintf(out,"+ order:\t");
    bdd_print_V_bddstr(&bdd->order,out);
    putc('\n',out);
    fprintf(out,"N=%d\n",bdd->n);
    bdd_print_tree(&bdd->core.tree,stdout);
}

bddstr bdd_get_rva_name(bddstr rva) {
    bddstr res = rva;
    
    char* p;

    if ( (p = strchr(res.str,'=')) )
        *p = 0;
    else
        vector_error("bad rva string, \'no\' = | %s",rva.str);
    return res;
}

int bdd_get_rva_value(bddstr rva) {
    char* p;

    if ( (p = strchr(rva.str,'=')) ) {
        int res = bdd_atoi(++p);
        if ( res == BDD_NONE )
            vector_error("bad rva string, bad value | %s",rva.str);
        return res;
    } else
        vector_error("bad rva string, \'no\' = | %s",rva.str);
    return BDD_NONE;
}

int bdd_low(bdd_runtime* bdd, int i) {
    return V_bddrow_get(&bdd->core.tree,i).low;
}

int bdd_high(bdd_runtime* bdd, int i) {
    return V_bddrow_get(&bdd->core.tree,i).high;
}

int bdd_is_leaf(bdd_runtime* bdd, int i) {
    return (i==0)||(i==1); // incomplete, check row is cleaner
}

static int bdd_lookup(bdd_runtime* bdd, char* rva, int low, int high) {
    bddrow findrow;

    // no hash like in python example, optimization will be later
    strncpy(findrow.rva,rva,MAXRVA);
    findrow.low  = low;
    findrow.high = high;
    return V_bddrow_find(&bdd->core.tree,cmpBddrow,&findrow);
} 

static int bdd_create_node(bdd_runtime* bdd, char* rva, int low, int high) {
    bddrow newrow;

    strncpy(newrow.rva,rva,MAXRVA);
    newrow.low  = low;
    newrow.high = high;
    return V_bddrow_add(&bdd->core.tree,newrow);
}

static int bdd_mk(bdd_runtime* bdd, char* v, int l, int h) {
    fprintf(stdout,"MK[v=\"%s\", l=%d, h=%d]\n",v,l,h);
    bdd->mk_calls++;
    if ( l == h )
        return h;
    int node = bdd_lookup(bdd,v,l,h);
    if ( !(node == -1) ) 
        return node; /* node already exists */ 
    node = bdd_create_node(bdd,v,l,h);
    return node;
} 

static int bdd_build_bdd(bdd_runtime* bdd, char* expr, int i, char* rewrite_buffer) {
    fprintf(stdout,"BUILD[i=%d]: %s\n",i,expr);

    if ( i >= bdd->n )
        return (bdd_eval_bool(expr) ? 1 : 0);
    bddstr var = V_bddstr_get(&bdd->order,i);
    char* newexpr = rewrite_buffer + (i*bdd->expr_bufflen); // Pretty brill:-)
    bdd_replace_str(newexpr,expr,var.str,"0");
    int l = bdd_build_bdd(bdd,newexpr,i+1,rewrite_buffer);
    bdd_replace_str(newexpr,expr,var.str,"1");
    int h = bdd_build_bdd(bdd,newexpr,i+1,rewrite_buffer);
    return bdd_mk(bdd,var.str,l,h);
}

void bdd_start_build(bdd_runtime* bdd) {
    fprintf(stdout,"BDD start_build\n");
    //
    V_bddrow_reset(&bdd->core.tree);
    bdd_create_node(bdd,"0",BDD_NONE,BDD_NONE);
    bdd_create_node(bdd,"1",BDD_NONE,BDD_NONE);
    bdd->mk_calls = 0;
    //
    fprintf(stdout,"/-------START--------\n");
    bdd_print(bdd,stdout);
    fprintf(stdout,"/--------------------\n");
    //
    bdd->expr_bufflen = strlen(bdd->core.expr)+1;
    char* rewrite_buffer = (char*)MALLOC((bdd->n * bdd->expr_bufflen));
    //
    bdd_build_bdd(bdd,bdd->core.expr,0,rewrite_buffer);
    //
    FREE(rewrite_buffer);
    //
    fprintf(stdout,"/------FINISH--------\n");
    bdd_print(bdd,stdout);
    fprintf(stdout,"/--------------------\n");
    //
    fprintf(stdout,"BDD start_build finish\n");
}

#define BDD_BASE_SIZE   (sizeof(bdd) - sizeof(V_bddrow))

static bdd* serialize_bdd(bdd* tbs) {
    int tree_size, expr_size, bytesize;

    V_bddrow_shrink2size(&tbs->tree);
    tree_size = V_bddrow_bytesize(&tbs->tree);
    expr_size = strlen(tbs->expr) + 1;
    bytesize  = BDD_BASE_SIZE + tree_size + expr_size;

    bdd* res = NULL;
    if ( (res = (bdd*)MALLOC(bytesize)) ) {
        res->bytesize = bytesize;
        V_bddrow_serialize(&res->tree,&tbs->tree);
        res->expr     = memcpy(((char*)&res->tree)+tree_size,tbs->expr,expr_size);
        return res;
    } 
    else
        return NULL; // incomplete, errmsg here
}

bdd* relocate_bdd(bdd* tbr) {
    V_bddrow_relocate(&tbr->tree);
    tbr->expr     = ((char*)&tbr->tree)+V_bddrow_bytesize(&tbr->tree);
    return tbr;
}

bdd* create_bdd(char* expr, char** _errmsg, int verbose) {
    bdd_runtime  bdd_struct;
    bdd_runtime* bdd_rt = bdd_init(&bdd_struct,expr,NULL,verbose);

    bdd_start_build(bdd_rt);
    bdd* res = serialize_bdd(&bdd_rt->core);
    bdd_free(bdd_rt);

    return res;
}
