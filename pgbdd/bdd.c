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

DefVectorC(rva);

int cmpRva(rva* l, rva* r) {  
        int res = strcmp(l->var,r->var);
        if ( res == 0 )
            res = (l->val - r->val);
        return res;
}

DefVectorC(rva_node);

int cmpRva_node(rva_node* l, rva_node* r) {
    int res = cmpRva(&l->rva,&r->rva);
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

static bdd_runtime* bdd_init(bdd_runtime* bdd, char* expr, int verbose, char** _errmsg) {
    if ( verbose ) 
        fprintf(stdout,"Create bdd: %s\n",expr);
    bdd->verbose = verbose;
    bdd->core.expr  = expr;
    if ( !bdd_set_default_order(&bdd->order,bdd->core.expr,_errmsg))  
        return NULL;
    V_rva_node_init(&bdd->core.tree);
    bdd->n     = V_rva_size(&bdd->order);
    bdd->mk_calls = 0;
    bdd->check_calls = 0;
    //
    return bdd;
}

void bdd_free(bdd_runtime* bdd) {
    V_rva_free(&bdd->order);
    V_rva_node_free(&bdd->core.tree);
}

static void bdd_print(bdd_runtime* bdd, pbuff* pbuff) {
    bprintf(pbuff,"+ expr     = %s\n",bdd->core.expr);
    bprintf(pbuff,"+ order    = ");
    bdd_print_V_rva(&bdd->order,pbuff);
    bprintf(pbuff,"\n");
    bprintf(pbuff,"N          = %d\n",bdd->n);
    bprintf(pbuff,"mk_calls   = %d\n",bdd->mk_calls);
    bprintf(pbuff,"check_calls= %d\n",bdd->check_calls);
    bprintf(pbuff,"Tree       = [\n");
    bdd_print_tree(&bdd->core.tree,pbuff);
    bprintf(pbuff,"]\n");
}

void bdd_info(bdd* bdd, pbuff* pbuff) {
    bprintf(pbuff,"%s\n\n",bdd->expr);
    bdd_print_tree(&bdd->tree,pbuff);
}

int rva_is_samevar(rva* l, rva* r) {
    return strcmp(l->var,r->var) == 0;
}

rva* bdd_rva(bdd* bdd, int i) {
    return &V_rva_node_getp(&bdd->tree,i)->rva;
}

int bdd_low(bdd* bdd, int i) {
    return V_rva_node_getp(&bdd->tree,i)->low;
}

int bdd_high(bdd* bdd, int i) {
    return V_rva_node_getp(&bdd->tree,i)->high;
}

int bdd_is_leaf(bdd* bdd, int i) {
    return (i==0)||(i==1); // incomplete, check row is cleaner
}

static int bdd_lookup(bdd_runtime* bdd, rva* rva, int low, int high) {
    // ORIGINAL: 
    // rva_node tofind = { .rva  = *rva, .low  = low, .high = high };
    // return V_rva_node_find(&bdd->core.tree,cmpRva_node,&tofind);
    // OPTIMIZED:
    V_rva_node *tree = &bdd->core.tree;
    for (int i=0; i<tree->size; i++) {
        rva_node* n = &tree->items[i];
        if ( (n->low == low) && 
             (n->high == high) &&
             (n->rva.val = rva->val) &&
             (strcmp((char*)&n->rva.var,(char*)&rva->var)==0)) {
            return i;
        }
    }
    return -1;
} 

static int bdd_create_node(bdd_runtime* bdd, rva* rva, int low, int high) {
    rva_node newrow = { .rva  = *rva, .low  = low, .high = high };
    return V_rva_node_add(&bdd->core.tree,newrow);
}

/*
 * bdd_alg is used to simulate subtyping/function inheritance
 */

static int bdd_mk_BASE(bdd_alg* alg, bdd_runtime* bdd, rva *v, int l, int h, char** _errmsg) {
    if ( bdd->verbose )
        fprintf(stdout,"MK{%s}[v=\"%s=%d\", l=%d, h=%d]\n",alg->name,v->var,v->val,l,h);
    bdd->mk_calls++;
    if ( l == h )
        return h;
    int node = bdd_lookup(bdd,v,l,h);
    if ( node != -1 ) 
        return node; /* node already exists */ 
    node = bdd_create_node(bdd,v,l,h);
    return node;
} 

static void create_rva_string(char* dst, rva* src) {
    char *s = src->var;
    while ( *s ) *dst++ = *s++;
    *dst++ = '=';
    if ( 0 ) {
        *dst   = 0;
        sprintf(dst,"%d",src->val);
    } else 
        fast_itoa(dst,src->val);
}

static int bdd_build_bdd_BASE(bdd_alg* alg, bdd_runtime* bdd, char* expr, int i, char* rewrite_buffer, char** _errmsg) {
    if ( bdd->verbose )
        fprintf(stdout,"BUILD{%s}[i=%d]: %s\n",alg->name,i,expr);
    if ( i >= bdd->n ) {
        if ( bdd->verbose )
            fprintf(stdout,"EVAL{%s}[i=%d]: %s = ",alg->name,i,expr);
        int res = bee_eval(expr,_errmsg);
        if ( bdd->verbose )
            fprintf(stdout,"%d\n",res);
        return res;
    }
    rva* var = V_rva_getp(&bdd->order,i);
    char* newexpr = rewrite_buffer + (i*bdd->expr_bufflen); // Pretty brill:-)
    char rva_string[MAX_RVA_LEN];
    create_rva_string(rva_string,var);
    bdd_replace_str(newexpr,expr,rva_string,'0');
    int l = alg->build(alg,bdd,newexpr,i+1,rewrite_buffer,_errmsg);
    bdd_replace_str(newexpr,expr,rva_string,'1');
    int h = alg->build(alg,bdd,newexpr,i+1,rewrite_buffer,_errmsg);
    if ( l<0 || h<0 )
        return -1;
    else
        return alg->mk(alg,bdd,var,l,h,_errmsg);
}

static int bdd_build_bdd_KAJ(bdd_alg* alg, bdd_runtime* bdd, char* expr, int i, char* rewrite_buffer, char** _errmsg) {
    if ( bdd->verbose )
        fprintf(stdout,"BUILD{%s}[i=%d]: %s\n",alg->name,i,expr);
    if ( i >= bdd->n ) {
        if ( bdd->verbose )
            fprintf(stdout,"EVAL{%s}[i=%d]: %s = ",alg->name,i,expr);
        int res = bee_eval(expr,_errmsg);
        if ( bdd->verbose )
            fprintf(stdout,"%d\n",res);
        return res;
    }
    rva* var = V_rva_getp(&bdd->order,i);
    char* newexpr = rewrite_buffer + (i*bdd->expr_bufflen); // Pretty brill:-)
    char rva_string[MAX_RVA_LEN];
    create_rva_string(rva_string,var);
    bdd_replace_str(newexpr,expr,rva_string,'0');
    int l = alg->build(alg,bdd,newexpr,i+1,rewrite_buffer,_errmsg);
    bdd_replace_str(newexpr,expr,rva_string,'1');
    //
    int dis = i+1;
    while ( dis < V_rva_size(&bdd->order) ) {
         rva* disvar = V_rva_getp(&bdd->order,dis);
         if ( !rva_is_samevar(var,disvar) )
             break; // break while
         if ( var->val != disvar->val ) {
             char dis_rva_string[MAX_RVA_LEN];
             create_rva_string(dis_rva_string,disvar);
             memcpy(expr,newexpr,bdd->expr_bufflen);
             bdd_replace_str(newexpr,expr,dis_rva_string,'0');
         }
         dis++;
    }
    int h = alg->build(alg,bdd,newexpr,dis/*newvar*/,rewrite_buffer,_errmsg);
    if ( l<0 || h<0 )
        return -1;
    else
        return alg->mk(alg,bdd,var,l,h,_errmsg);
}

static rva RVA_0 = {.var = "0", .val = -1};
static rva RVA_1 = {.var = "1", .val = -1};

static int bdd_start_build(bdd_alg* alg, bdd_runtime* bdd, char** _errmsg) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    if ( bdd->verbose )
        fprintf(stdout,"BDD start_build\n");
    //
    V_rva_node_reset(&bdd->core.tree);
    bdd_create_node(bdd,&RVA_0,BDD_NONE,BDD_NONE);
    bdd_create_node(bdd,&RVA_1,BDD_NONE,BDD_NONE);
    bdd->mk_calls = 0;
    //
    if ( bdd->verbose ) {
        fprintf(stdout,"/-------START, alg=\"%s\"--------\n",alg->name);
        bdd_print(bdd,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
    bdd->expr_bufflen = strlen(bdd->core.expr)+1;
    char* rewrite_buffer = (char*)MALLOC(((bdd->n+1) * bdd->expr_bufflen));
    // the last buffer is for the expression self
    char* exprbuff       = rewrite_buffer + (bdd->n*bdd->expr_bufflen);
    memcpy(exprbuff,bdd->core.expr,bdd->expr_bufflen);
    // Remove all spaces from the buffer! This will result in final expressions
    // with only characters 01&|!(). This way we can build an extremely fast
    // expression evaluator
    bdd_remove_spaces(exprbuff);
    //
    int res = alg->build(alg,bdd,exprbuff,0,rewrite_buffer,_errmsg);
    //
    FREE(rewrite_buffer);
    //
    if ( bdd->verbose ) {
        fprintf(stdout,"/------FINISH, alg=\"%s\"--------\n",alg->name);
        if ( res < 0 ) 
            fprintf(stdout,"ERROR: res=%d\n",res);
        else
            bdd_print(bdd,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
    //
    pbuff_free(&pbuff_struct);
    //
    return (res<0) ? 0 : 1;
}

bdd_alg S_BDD_BASE    = {.name = "BASE", .build = bdd_build_bdd_BASE, .mk = bdd_mk_BASE};
bdd_alg S_BDD_KAJ     = {.name = "KAJ" , .build = bdd_build_bdd_KAJ,  .mk = bdd_mk_BASE};
bdd_alg S_BDD_PROBBDD = {.name = "PROBBDD", .build = bdd_build_bdd_BASE, .mk = bdd_mk_BASE};

bdd_alg *BDD_BASE    = &S_BDD_BASE;
bdd_alg *BDD_KAJ     = &S_BDD_KAJ;
bdd_alg *BDD_PROBBDD = &S_BDD_PROBBDD;

bdd* create_bdd(bdd_alg* alg, char* expr, char** _errmsg, int verbose) {
    bdd_runtime  bdd_struct;
    bdd_runtime* bdd_rt;

    if ( !(bdd_rt = bdd_init(&bdd_struct,expr,verbose,_errmsg)) )
        return NULL;
    if ( ! bdd_start_build(alg,bdd_rt,_errmsg) ) {
        bdd_free(bdd_rt);
        return NULL;
    }
    bdd* res = serialize_bdd(&bdd_rt->core);
    bdd_free(bdd_rt);

    return res;
}

/*
 *
 */

#define BDD_BASE_SIZE   (sizeof(bdd) - sizeof(V_rva_node))

bdd* serialize_bdd(bdd* tbs) {
    int tree_size, expr_size, bytesize;

    V_rva_node_shrink2size(&tbs->tree);
    tree_size = V_rva_node_bytesize(&tbs->tree);
    expr_size = strlen(tbs->expr) + 1;
    bytesize  = BDD_BASE_SIZE + tree_size + expr_size;

    bdd* res = NULL;
    if ( (res = (bdd*)MALLOC(bytesize)) ) {
        res->bytesize = bytesize;
        V_rva_node_serialize(&res->tree,&tbs->tree);
        res->expr     = memcpy(((char*)&res->tree)+tree_size,tbs->expr,expr_size);
        return res;
    } 
    else
        return NULL; // incomplete, errmsg here
}

bdd* relocate_bdd(bdd* tbr) {
    V_rva_node_relocate(&tbr->tree);
    tbr->expr     = ((char*)&tbr->tree)+V_rva_node_bytesize(&tbr->tree);
    return tbr;
}

//
//
//

int create_rva(rva* res, char* val, int len, char** _errmsg) {
    char* p = val;
    char* d = res->var;
    while ( *p && isspace(*p) ) p++;
    while (*p && !isspace(*p) && *p != '=')
        *d++ = *p++;
    if ( *p && isspace(*p) )
        while ( *p && isspace(*p) ) p++;
    *d = 0;
    if ( *p != '=' ) {
        if ( strcmp(res->var,"not")==0||strcmp(res->var,"and")==0||strcmp(res->var,"or")==0 )
            return pg_error(_errmsg, "bdd-expr: do not use and/or/not keywords but use '&|!': %s",val);
        else 
            return pg_error(_errmsg,"bad rva string, \'no\' = | %s",val);
    }
    if ( (res->val = bdd_atoi(++p)) == BDD_NONE )
        return pg_error(_errmsg,"bad rva string, bad value | %s",val);
    return 1;
}

#define is_rva_char(C) (isalnum(C)||C=='=')

int bdd_set_default_order(V_rva* order, char* expr, char** _errmsg) {
    char *p = expr;
    V_rva_init(order);
    do {
        while (*p && !is_rva_char(*p)) p++;
        if ( *p) {
            char* start=p; 

            // incomplete, alg does not allow space around = now
            while (*p && is_rva_char(*p)) p++; 
            rva s;
            if ( !create_rva(&s,start,p-start,_errmsg) )
                return 0;
            V_rva_add(order,s);
        }
    } while (*p);
    // now sort the result string and make result unique
    if ( V_rva_size(order) > 0) {
        V_rva_quicksort(order,0,order->size-1,cmpRva);
        rva* last = V_rva_getp(order,0);
        for(int i=1; i<V_rva_size(order); i++) {
            rva* curstr = V_rva_getp(order,i);
            if ( cmpRva(last,curstr)==0 )
                V_rva_delete(order,i--); // remove i'th element and decrease i
            else
                last = curstr;
        }
    }
    return 1;
}

void bdd_print_V_rva(V_rva* v, pbuff* pbuff) {
    bprintf(pbuff,"{");
    for(int i=0; i<V_rva_size(v); i++) {
        rva* rva = V_rva_getp(v,i);
        bprintf(pbuff,"%s=%d ",rva->var,rva->val);
    }
    bprintf(pbuff,"}");
}

/*
 *
 */

static void bdd_print_row(rva_node* row, pbuff* pbuff) {
    if ( row->rva.val < 0 )
        bprintf(pbuff,"[\"%s\"]\n",row->rva.var);
    else
        bprintf(pbuff,"[\"%s=%d\",%d,%d]\n",row->rva.var,row->rva.val,row->low,row->high);
}

void bdd_print_tree(V_rva_node* tree, pbuff* pbuff) {
    for(int i=0; i<V_rva_node_size(tree); i++) {
        bprintf(pbuff,"\t%d:\t",i);
        bdd_print_row(V_rva_node_getp(tree,i),pbuff);
    }
}

static void generate_label(pbuff* pbuff, int i, rva* base, char* extra) {
    if ( base->val < 0 )
        bprintf(pbuff,"\t%d [label=<<b>%s</b>",i,base->var);
    else
        bprintf(pbuff,"\t%d [label=<<b>%s=%d</b>",i,base->var,base->val);
    if ( extra )
        bprintf(pbuff,"%s",extra);
    bprintf(pbuff,">]\n");
}

void bdd_generate_dot(bdd* bdd, pbuff* pbuff, char** extra) {
    V_rva_node* tree = &bdd->tree;

    bprintf(pbuff,"digraph {\n");
    bprintf(pbuff,"\tlabelloc=\"t\";\n");
    bprintf(pbuff,"\tlabel=\"bdd(\'%s\')\";\n",bdd->expr);
    for(int i=0; i<V_rva_node_size(tree); i++) {
        rva_node row = V_rva_node_get(tree,i);
        if ( i<2 ) {
            bprintf(pbuff,"\tnode [shape=square]\n");
            generate_label(pbuff,i,&row.rva,(extra ? extra[i] : NULL));
        } else {
            bprintf(pbuff,"\tnode [shape=circle]\n");
            generate_label(pbuff,i,&row.rva,(extra ? extra[i] : NULL));
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
 */

static double bdd_probability_node(bdd_dictionary* dict, bdd* bdd, int i,char** extra,int verbose,char** _errmsg) {
    rva *rva = bdd_rva(bdd,i);
    double res;

    // incomplete: highly unefficient, store already computed results
    if (verbose )
        fprintf(stdout,"+ bdd_probability(node=%d,\'%s=%d\')\n",i,rva->var,rva->val);
    double p_root;
    if ( bdd_is_leaf(bdd,i) ) {
        res = p_root = (i==0) ? 0.0 : 1.0;
        if ( verbose )
            fprintf(stdout,"++ is_leaf: P=%f\n",res);
    } else {
        p_root = dictionar_lookup_prob(dict,rva);
        if ( p_root < 0.0 ) {
            pg_error(_errmsg,"dictionary_lookup: rva[\'%s\'] not found.",rva);
            return -1.0;
        }
        int low  = bdd_low(bdd,i);
        int high = bdd_high(bdd,i);
        if ( verbose )
            fprintf(stdout,"++ is_node(low=%d, high=%d)\n",low,high);
        double p_l = bdd_probability_node(dict,bdd,low,extra,verbose,_errmsg);
        double p_h = bdd_probability_node(dict,bdd,high,extra,verbose,_errmsg);
        if ( p_l < 0.0 || p_h < 0.0 )
            return -1.0;
        res = (p_root * p_h) + p_l;
        if ( ! rva_is_samevar(rva,bdd_rva(bdd,low)) )
            res = res - p_root*p_l;
        if ( verbose )
            fprintf(stdout,"+ bdd_probability(node=%d,\'%s=%d\') p_root=%f, P=%f\n",i,rva->var,rva->val,p_root,res);
   }
   if ( extra )
       sprintf(extra[i],"<i>(%.2f)<br/>%.2f</i>",p_root,res);
   return res;
}

double bdd_probability(bdd_dictionary* dict, bdd* bdd,char** extra, int verbose, char** _errmsg) {
    int topnode = V_rva_node_size(&bdd->tree) - 1;
    
    return bdd_probability_node(dict,bdd,topnode,extra,verbose,_errmsg);
}
