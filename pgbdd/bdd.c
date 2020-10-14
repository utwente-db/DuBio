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

int cmpBddstr(bddstr l, bddstr r) {  return strcmp(l.str,r.str); }

//

DefVectorC(bddrow);

int cmpBddrow(bddrow l, bddrow r) {
    int res = strcmp(l.rva, r.rva);
    if (res == 0) {
        res = l.low - r.low;
        if ( res == 0 )
            res = l.high - r.high;
    }
    return res;
} 

/*
 *
 *
 */

static bdd* bdd_init(bdd* bdd, char* expr, char* name, V_bddstr* order, int verbose) {
    fprintf(stdout,"Create bdd\n");
    strncpy(bdd->name,(name?name:"NONAME"),MAXRVA);
    bdd->verbose = 1;
    V_bddrow_init(&bdd->tree);
    //
    bdd->expr  = expr;
    bdd->order = (order ? *order :bdd_set_default_order(bdd->expr));
    bdd->n     = V_bddstr_size(&bdd->order);
    bdd->mk_calls = 0;
    //
    return bdd;
}

static void bdd_free(bdd* bdd) {
    V_bddstr_free(&bdd->order);
    V_bddrow_free(&bdd->tree);
}

static void bdd_print(bdd* bdd, FILE* out) {
    fprintf(out,"BDD: %s\n",bdd->name);
    fprintf(out,"+ expr:\t%s\n",bdd->expr);
    fprintf(out,"+ order:\t");
    bdd_print_V_bddstr(&bdd->order,out);
    putc('\n',out);
    fprintf(out,"N=%d\n",bdd->n);
    bdd_print_tree(&bdd->tree,stdout);
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

int bdd_low(bdd* bdd, int i) {
    return V_bddrow_get(&bdd->tree,i).low;
}

int bdd_high(bdd* bdd, int i) {
    return V_bddrow_get(&bdd->tree,i).high;
}

int bdd_is_leaf(bdd* bdd, int i) {
    return (i==0)||(i==1); // incomplete, check row is cleaner
}

static int bdd_lookup(bdd* bdd, char* rva, int low, int high) {
    bddrow findrow;

    // no hash like in python, optimization will be later
    strncpy(findrow.rva,rva,MAXRVA);
    findrow.low  = low;
    findrow.high = high;
    return V_bddrow_find(&bdd->tree,cmpBddrow,findrow);
} 

static int bdd_create_node(bdd* bdd, char* rva, int low, int high) {
    bddrow newrow;

    strncpy(newrow.rva,rva,MAXRVA);
    newrow.low  = low;
    newrow.high = high;
    return V_bddrow_add(&bdd->tree,newrow);
}

static int bdd_mk(bdd* bdd, char* v, int l, int h) {
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

static int bdd_build_bdd(bdd* bdd, char* expr, int i, char* rewrite_buffer) {
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

static void bdd_start_build(bdd* bdd) {
    fprintf(stdout,"BDD start_build\n");
    //
    V_bddrow_reset(&bdd->tree);
    bdd_create_node(bdd,"0",BDD_NONE,BDD_NONE);
    bdd_create_node(bdd,"1",BDD_NONE,BDD_NONE);
    bdd->mk_calls = 0;
    //
    fprintf(stdout,"/-------START--------\n");
    bdd_print(bdd,stdout);
    fprintf(stdout,"/--------------------\n");
    //
    bdd->expr_bufflen = strlen(bdd->expr)+1;
    char* rewrite_buffer = (char*)MALLOC((bdd->n * bdd->expr_bufflen));
    //
    bdd_build_bdd(bdd,bdd->expr,0,rewrite_buffer);
    //
    FREE(rewrite_buffer);
    //
    fprintf(stdout,"/------FINISH--------\n");
    bdd_print(bdd,stdout);
    fprintf(stdout,"/--------------------\n");
    char* dotfile = "./DOT/test.dot";
    if ( dotfile ) {
        bdd_generate_dot(bdd,dotfile);
        fprintf(stdout,"Generated dotfile: %s\n",dotfile); 
    }
    //
    fprintf(stdout,"BDD start_build finish\n");
}

/*
 *
 *
 */

#ifdef TEST_CONFIG

void test_bdd(){
    fprintf(stdout,"Test bdd\n");
    // char* expr = "((x=1 and x=1) or (not y=0 and not zzz=1)) and((xx=2 and x=3)or(not x=2 and not x=3))";
    // char* expr = "((x=1 | x=2) & x=2)";
    char* expr = "x=1 & x=2 | x=3 & x=4 | x=5 & x=6 | x=7 & x=8";
    // (x0 and y1) or (not x0 and not y1) or ((x2 and y2) and y34)
    // char* expr = "(x=1 | x=2)";
    bdd bdd_struct;
    bdd* bdd = bdd_init(&bdd_struct,expr,NULL,NULL,1);
    //
    bdd_start_build(bdd);
    //
    bdd_free(bdd);
}

#endif
