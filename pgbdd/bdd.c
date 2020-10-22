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
#include <ctype.h>

#include "vector.h"
#include "utils.h"
#include "dictionary.h"
#include "bdd.h"

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
    if ( verbose ) 
        fprintf(stdout,"Create bdd: %s\n",expr);
    bdd->verbose = verbose;
    V_bddrow_init(&bdd->core.tree);
    //
    bdd->core.expr  = expr;
    bdd->order = (order ? *order:bdd_set_default_order(bdd->core.expr));
    bdd->n     = V_bddstr_size(&bdd->order);
    bdd->mk_calls = 0;
    //
    return bdd;
}

void bdd_free(bdd_runtime* bdd) {
    V_bddstr_free(&bdd->order);
    V_bddrow_free(&bdd->core.tree);
}

static void bdd_print(bdd_runtime* bdd, pbuff* pbuff) {
    bprintf(pbuff,"BDD: \n");
    bprintf(pbuff,"+ expr:\t%s\n",bdd->core.expr);
    bprintf(pbuff,"+ order:\t");
    bdd_print_V_bddstr(&bdd->order,pbuff);
    bprintf(pbuff,"\n");
    bprintf(pbuff,"N=%d\n",bdd->n);
    bdd_print_tree(&bdd->core.tree,pbuff);
}

void bdd_info(bdd* bdd, pbuff* pbuff) {
    bprintf(pbuff,"%s\n\n",bdd->expr);
    bdd_print_tree(&bdd->tree,pbuff);
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

char* bdd_rva(bdd* bdd, int i) {
    return V_bddrow_getp(&bdd->tree,i)->rva;
}

int bdd_low(bdd* bdd, int i) {
    return V_bddrow_getp(&bdd->tree,i)->low;
}

int bdd_high(bdd* bdd, int i) {
    return V_bddrow_getp(&bdd->tree,i)->high;
}

int bdd_is_leaf(bdd* bdd, int i) {
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
    if ( bdd->verbose )
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
    if ( bdd->verbose )
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
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    if ( bdd->verbose )
        fprintf(stdout,"BDD start_build\n");
    //
    V_bddrow_reset(&bdd->core.tree);
    bdd_create_node(bdd,"0",BDD_NONE,BDD_NONE);
    bdd_create_node(bdd,"1",BDD_NONE,BDD_NONE);
    bdd->mk_calls = 0;
    //
    if ( bdd->verbose ) {
        fprintf(stdout,"/-------START--------\n");
        bdd_print(bdd,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------\n");
    }
    bdd->expr_bufflen = strlen(bdd->core.expr)+1;
    char* rewrite_buffer = (char*)MALLOC((bdd->n * bdd->expr_bufflen));
    //
    bdd_build_bdd(bdd,bdd->core.expr,0,rewrite_buffer);
    //
    FREE(rewrite_buffer);
    //
    if ( bdd->verbose ) {
        fprintf(stdout,"/------FINISH--------\n");
        bdd_print(bdd,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------\n");
    }
    //
    pbuff_free(&pbuff_struct);
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

//
//
//

bddstr create_bddstr(char* val, int len) {
    bddstr newstr;

    if ( len >= MAXRVA )
        vector_error("RVA string too large");
    memcpy(newstr.str,val,len);
    newstr.str[len] = 0;
    return newstr;
}

void bdd_print_V_bddstr(V_bddstr* v, pbuff* pbuff) {
    bprintf(pbuff,"{");
    for(int i=0; i<V_bddstr_size(v); i++) {
        bprintf(pbuff,"%s ",V_bddstr_get(v,i).str);
    }
    bprintf(pbuff,"}");
}

#define is_rva_char(C) (isalnum(C)||C=='=')

V_bddstr bdd_set_default_order(char* expr) {
    V_bddstr res;

    V_bddstr_init(&res);
    char*p = expr;
    do {
        while (*p && !is_rva_char(*p)) p++;
        if ( *p) {
            char* start=p; 

            while (*p && is_rva_char(*p)) p++; 
            bddstr s = create_bddstr(start,p-start);
            if ( !(strcmp(s.str,"not")==0||strcmp(s.str,"and")==0||strcmp(s.str,"or")==0) )
                V_bddstr_add(&res,s);
        }
    } while (*p);
    // now sort the result string and make result unique
    if ( V_bddstr_size(&res) > 0) {
        V_bddstr_quicksort(&res,0,res.size-1,cmpBddstr);
        bddstr* last = V_bddstr_getp(&res,0);
        for(int i=1; i<V_bddstr_size(&res); i++) {
            bddstr* curstr = V_bddstr_getp(&res,i);
            if ( cmpBddstr(last,curstr)==0 )
                V_bddstr_delete(&res,i--); // remove i'th element and decrease i
            else
                last = curstr;
        }
    }
    return res;
}

static void bdd_print_row(bddrow row, pbuff* pbuff) {
        bprintf(pbuff,"[\"%s\",%d,%d]\n",row.rva,row.low,row.high);
}

void bdd_print_tree(V_bddrow* tree, pbuff* pbuff) {
    for(int i=0; i<V_bddrow_size(tree); i++) {
        bprintf(pbuff,"%d:\t",i);
        bdd_print_row(V_bddrow_get(tree,i),pbuff);
    }
}

static void generate_label(pbuff* pbuff, int i, char* base, char* extra) {
    bprintf(pbuff,"\t%d [label=<<b>%s</b>",i,base);
    if ( extra )
        bprintf(pbuff,"%s",extra);
    bprintf(pbuff,">]\n");
}

void bdd_generate_dot(bdd* bdd, pbuff* pbuff, char** extra) {
    V_bddrow* tree = &bdd->tree;

    bprintf(pbuff,"digraph {\n");
    bprintf(pbuff,"\tlabelloc=\"t\";\n");
    bprintf(pbuff,"\tlabel=\"bdd(\'%s\')\";\n",bdd->expr);
    for(int i=0; i<V_bddrow_size(tree); i++) {
        bddrow row = V_bddrow_get(tree,i);
        if ( i<2 ) {
            bprintf(pbuff,"\tnode [shape=square]\n");
            generate_label(pbuff,i,row.rva,(extra ? extra[i] : NULL));
        } else {
            bprintf(pbuff,"\tnode [shape=circle]\n");
            generate_label(pbuff,i,row.rva,(extra ? extra[i] : NULL));
            bprintf(pbuff,"\tedge [shape=rarrow style=dashed]\n");
            bprintf(pbuff,"\t%d -> %d\n",i,row.low);
            bprintf(pbuff,"\tedge [shape=rarrow style=bold]\n");
            bprintf(pbuff,"\t%d -> %d\n",i,row.high);
        }
    }
    bprintf(pbuff,"}\n");
}

void bdd_generate_dotfile(bdd* bdd, char* dotfile, char** extra) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_generate_dot(bdd,pbuff,extra);
    FILE* f = fopen(dotfile,"w");
    if ( f ) {
        fputs(pbuff->buffer,f);
        fclose(f);
    }
    pbuff_free(pbuff);
}

/*
 *
 *
 */

static double bdd_probability_node(bdd_dictionary* dict, bdd* bdd, int i,char** extra,int verbose,char** _errmsg) {
    char *rva = bdd_rva(bdd,i);
    double res;

    // incomplete: highly unefficient, store already computed results
    if (verbose )
        fprintf(stdout,"+ bdd_probability(node=%d,\'%s\')\n",i,rva);
    double p_root;
    if ( bdd_is_leaf(bdd,i) ) {
        res = p_root = (i==0) ? 0.0 : 1.0;
        if ( verbose )
            fprintf(stdout,"++ is_leaf: P=%f\n",res);
    } else {
        p_root = bdd_dictionary_lookup_rva_prob(dict,rva);
        if ( p_root < 0.0 ) {
            pg_error(_errmsg,"dictionary_lookup: rva[\'%s\'] not found.",rva);
            return -1.0;
        }
        int low = bdd_low(bdd,i);
        int high = bdd_high(bdd,i);
        if ( verbose )
            fprintf(stdout,"++ is_node(low=%d, high=%d)\n",low,high);
        double p_l = bdd_probability_node(dict,bdd,low,extra,verbose,_errmsg);
        double p_h = bdd_probability_node(dict,bdd,high,extra,verbose,_errmsg);
        if ( p_l < 0.0 || p_h < 0.0 )
            return -1.0;
        res = (p_root * p_h) + p_l;
        if ( ! rva_samevar(rva,bdd_rva(bdd,low)) )
            res = res - p_root*p_l;
        if ( verbose )
            fprintf(stdout,"+ bdd_probability(node=%d,\'%s\') p_root=%f, P=%f\n",i,rva,p_root,res);
   }
   if ( extra )
       sprintf(extra[i],"<i>(%.2f)<br/>%.2f</i>",p_root,res);
   return res;
}

double bdd_probability(bdd_dictionary* dict, bdd* bdd,char** extra, int verbose, char** _errmsg) {
    int topnode = V_bddrow_size(&bdd->tree) - 1;
    
    return bdd_probability_node(dict,bdd,topnode,extra,verbose,_errmsg);
}
