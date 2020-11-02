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

#include "utils.h"
#include "vector.h"
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
#ifdef BDD_VERBOSE
    if ( verbose ) 
        fprintf(stdout,"Create bdd: %s\n",expr);
    bdd->verbose     = verbose;
    bdd->mk_calls    = 0;
    bdd->check_calls = 0;
#endif
    bdd->core.expr  = expr;
    return bdd;
}

void bdd_free(bdd_runtime* bdd) {
    V_rva_free(&bdd->order);
    V_rva_node_free(&bdd->core.tree);
}

static void bdd_print_row(rva_node* row, pbuff* pbuff) {
    if ( row->rva.val < 0 )
        bprintf(pbuff,"[\"%s\"]\n",row->rva.var);
    else
        bprintf(pbuff,"[\"%s=%d\",%d,%d]\n",row->rva.var,row->rva.val,row->low,row->high);
}

static void bdd_print_tree(V_rva_node* tree, pbuff* pbuff) {
    for(int i=0; i<V_rva_node_size(tree); i++) {
        bprintf(pbuff,"\t%d:\t",i);
        bdd_print_row(V_rva_node_getp(tree,i),pbuff);
    }
}

#ifdef BDD_VERBOSE
static void bdd_print_V_rva(V_rva* v, pbuff* pbuff) {
    bprintf(pbuff,"{");
    for(int i=0; i<V_rva_size(v); i++) {
        rva* rva = V_rva_getp(v,i);
        bprintf(pbuff,"%s=%d ",rva->var,rva->val);
    }
    bprintf(pbuff,"}");
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
#endif

void bdd_info(bdd* bdd, pbuff* pbuff) {
    bprintf(pbuff,"%s\n\n",bdd->expr);
    bdd_print_tree(&bdd->tree,pbuff);
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
    return V_rva_node_add(&bdd->core.tree,&newrow);
}

/*
 *
 */

static int analyze_expr(char* expr, int* p_length, int* p_n_rva, int* p_n_spaces, char** _errmsg) {
    char c;
    char *p = expr;

    int length, n_rva, n_spaces;
    length = n_rva = n_spaces= 0;
    while ( (c=*p++) ) {
        length++;
        if ( isspace(c) )
            n_spaces++;
        else
            if ( c == '=' ) 
                n_rva++;
    }
    *p_length   = length;
    *p_n_rva    = n_rva;
    *p_n_spaces = n_spaces;
    //
    return 1;
}

static void bdd_strcpy_nospaces(char* dst, char* p, int max) {
    int cnt = 0;
    while (*p) {
        if ( isspace(*p) )
            p++;
        else {
            if ( ++cnt > max )
                pg_fatal(":bdd_strcpy_nospaces: output string too large");
            *dst++ = *p++;
        }
    }
    *dst = 0;
}

static int create_new_rva(rva* res, char* var, int var_len, char* val, char** _errmsg) {
    if ( var_len > MAX_RVA_NAME )
        return pg_error(_errmsg,"rva_name too long (max=%d) / %s",MAX_RVA_NAME, var);   
    memcpy(res->var,var,var_len);
    res->var[var_len] = 0;
    if ( strcmp(res->var,"not")==0||strcmp(res->var,"and")==0||strcmp(res->var,"or")==0 )
        return pg_error(_errmsg, "bdd-expr: do not use and/or/not keywords but use '&|!': %s",var);
    if ( (res->val = bdd_atoi(val)) == BDD_NONE )
        return pg_error(_errmsg,"bad rva string, bad value %s= %s",res->var,val);   
    return 1;
}

static int add_to_order(V_rva* order, rva* new_rva) {
// #define KEEP_ORDER_SORTED
#ifdef KEEP_ORDER_SORTED
    int index = V_rva_bsearch(order,cmpRva,new_rva);
    if ( index < 0 ) {
        index = -(index + VECTOR_BSEARCH_NEG_OFFSET);
        return (V_rva_insert_at(order,index,new_rva)<0) ? 0 : 1;
    } else
        return 1; // OK, already in order
#else
    return (V_rva_add(order,new_rva)<0) ? 0 : 1;
#endif
}

static int compute_default_order(V_rva* order, char* expr, char** _errmsg) {
    char *p = expr;
    
    while ( *p ) {
        while ( *p && !isalnum(*p) )
            p++;
        if ( isalnum(*p) ) {
            char* start = p;
            //
            if ( isdigit(*p) ) {
                if ( (*p=='0') || (*p=='1') ) {
                    if ( isalnum(p[1]) )
                        return pg_error(_errmsg,"varnames cannot start with a digit: \"%s\"",p);
                    else {
                        p++;
                    }
                } else 
                    return pg_error(_errmsg,"varnames cannot start with a digit: \"%s\"",p);
            } else {
                while ( isalnum(*p) )
                    p++;
                int len = p-start;;
                while ( *p && *p != '=' )
                    p++;
                if ( !(*p++ == '=') )
                    return pg_error(_errmsg,"missing \'=\' in expr: \"%s\"",start);
                while ( isspace(*p) ) 
                    p++;
                if ( !isdigit(*p) ) 
                    return pg_error(_errmsg,"missing value after \'=\' in expr: \"%s\"",start);
                rva new_rva;
                if ( !create_new_rva(&new_rva,start,len,p,_errmsg) )
                    return 0;
                if ( !add_to_order(order,&new_rva) )
                    return 0;
                while (isdigit(*p) )
                    p++;
            }
        }
    }
#ifndef KEEP_ORDER_SORTED
    // order was not kept sorted and uniq so we have to do this manually
    if ( V_rva_size(order) > 1) {
        V_rva_quicksort(order,cmpRva);
        rva* last = V_rva_getp(order,0);
        for(int i=1; i<V_rva_size(order); i++) {
            rva* curstr = V_rva_getp(order,i);
            if ( cmpRva(last,curstr)==0 )
                V_rva_delete(order,i--); // remove i'th element and decrease i
            else
                last = curstr;
        }
    }
#endif
    return 1;
}

/*
 * The BASE build()/mk() algorithms
 */

static int bdd_mk_BASE(bdd_alg* alg, bdd_runtime* bdd, rva *v, int l, int h, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd->verbose )
        fprintf(stdout,"MK{%s}[v=\"%s=%d\", l=%d, h=%d]\n",alg->name,v->var,v->val,l,h);
    bdd->mk_calls++;
#endif
    if ( l == h )
        return h;
    int node = bdd_lookup(bdd,v,l,h);
    if ( node != -1 ) 
        return node; /* node already exists */ 
    return bdd_create_node(bdd,v,l,h);
} 

static void create_rva_string(char* dst, rva* src) {
    char *s = src->var;
    while ( *s ) *dst++ = *s++;
    *dst++ = '=';
    if ( 1 ) {
        fast_itoa(dst,src->val);
    } else {
        *dst   = 0;
        sprintf(dst,"%d",src->val);
    }
}

static int bdd_build_bdd_BASE(bdd_alg* alg, bdd_runtime* bdd, char* expr, int i, char* rewrite_buffer, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd->verbose )
        fprintf(stdout,"BUILD{%s}[i=%d]: %s\n",alg->name,i,expr);
#endif
    if ( i >= bdd->n ) {
#ifdef BDD_VERBOSE
        if ( bdd->verbose )
            fprintf(stdout,"EVAL{%s}[i=%d]: %s = ",alg->name,i,expr);
#endif
        int res = bee_eval(expr,_errmsg);
#ifdef BDD_VERBOSE
        if ( bdd->verbose )
            fprintf(stdout,"%d\n",res);
#endif
        return res;
    }
    rva* var = V_rva_getp(&bdd->order,i);
    char* newexpr = rewrite_buffer + (i*bdd->len_expr); // Pretty brill:-)
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

/*
 * Kaj's algorithm
 */

static int bdd_build_bdd_KAJ(bdd_alg* alg, bdd_runtime* bdd, char* expr, int i, char* rewrite_buffer, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd->verbose )
        fprintf(stdout,"BUILD{%s}[i=%d]: %s\n",alg->name,i,expr);
#endif
    if ( i >= bdd->n ) {
#ifdef BDD_VERBOSE
        if ( bdd->verbose )
            fprintf(stdout,"EVAL{%s}[i=%d]: %s = ",alg->name,i,expr);
#endif
        int res = bee_eval(expr,_errmsg);
#ifdef BDD_VERBOSE
        if ( bdd->verbose )
            fprintf(stdout,"%d\n",res);
#endif
        return res;
    }
    rva* var = V_rva_getp(&bdd->order,i);
    char* newexpr = rewrite_buffer + (i*bdd->len_expr); // Pretty brill:-)
    char rva_string[MAX_RVA_LEN];
    create_rva_string(rva_string,var);
    bdd_replace_str(newexpr,expr,rva_string,'0');
    int l = alg->build(alg,bdd,newexpr,i+1,rewrite_buffer,_errmsg);
    if ( l<0 ) return -1;
    bdd_replace_str(newexpr,expr,rva_string,'1');
    //
    int scan_samevar_i = i+1;
    while ( scan_samevar_i < V_rva_size(&bdd->order) ) {
         rva* next_in_order = V_rva_getp(&bdd->order,scan_samevar_i);
         if ( !IS_SAMEVAR(var,next_in_order) )
             break; // varname changed, continue build from this index
         if ( var->val != next_in_order->val ) {
             char to_be_zerod[MAX_RVA_LEN];
             create_rva_string(to_be_zerod,next_in_order);
             memcpy(expr,newexpr,bdd->len_expr);
             bdd_replace_str(newexpr,expr,to_be_zerod,'0');
         }
         scan_samevar_i++;
    }
    int h = alg->build(alg,bdd,newexpr,scan_samevar_i/*newvar*/,rewrite_buffer,_errmsg);
    if ( h<0 )
        return -1;
    else
        return alg->mk(alg,bdd,var,l,h,_errmsg);
}

/*
 * The ROBDD algorithms
 *
 * INCOMPLETE: alg is wrong!!! "(z=1)&!((x=1)&((y=1|y=2)&x=2))" = 0 <> "z=1"
 *
 */

static void bdd_check_ROBDD(bdd_alg* alg, bdd_runtime* bdd_rt, int root, rva* iv, int l, int h, char** _errmsg) {
    bdd* bdd = &bdd_rt->core;
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose )
        fprintf(stdout,"CHECK{%s}[root=%d, l=%d, h=%d]\n",alg->name,root,l,h);
    bdd_rt->check_calls++;
#endif
    int low_branch  = 1;
    int high_branch = 1;
    if ( root != 0 && root != 1 ) {
        high_branch = bdd_high(bdd,root);
        low_branch  = bdd_low(bdd,root);
    } else
        return;
    if ( IS_LEAF_I(bdd,bdd_high(bdd,root) ) &&
         IS_LEAF_I(bdd,bdd_low(bdd,root) ) &&
         IS_LAST_ADDED(bdd,root)  ) {
        DELETE_LAST_ELEMENT(bdd);
    } else if (IS_LEAF_I(bdd,bdd_high(bdd,root) ) &&
               IS_LEAF_I(bdd,bdd_low(bdd,root) ) ) {
        return;
    } else if (IS_LAST_ADDED(bdd,root) ) {
        if (IS_LAST_ADDED(bdd,h) )
            return;
        DELETE_LAST_ELEMENT(bdd);
        if ( high_branch > low_branch ) {
            bdd_check_ROBDD(alg,bdd_rt,high_branch,iv,l,h,_errmsg);
            bdd_check_ROBDD(alg,bdd_rt, low_branch,iv,l,h,_errmsg);
        } else {
            bdd_check_ROBDD(alg,bdd_rt, low_branch,iv,l,h,_errmsg);
            bdd_check_ROBDD(alg,bdd_rt,high_branch,iv,l,h,_errmsg);
        }
    }
}

static int bdd_mk_ROBDD(bdd_alg* alg, bdd_runtime* bdd_rt, rva *iv, int l, int h, char** _errmsg) {
    bdd* bdd = &bdd_rt->core;
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose )
        fprintf(stdout,"MK{%s}[v=\"%s=%d\", l=%d, h=%d]\n",alg->name,iv->var,iv->val,l,h);
    bdd_rt->mk_calls++;
#endif
    int flag = 0;
    if ( l == h ) return l;
    else {
        int index = bdd_lookup(bdd_rt,iv,l,h);
        if ( index >= 0 ) 
            return index;
        if ( h != 0 && h != 1 && flag == 0) {
            while ( IS_SAMEVAR(&(bdd_node(bdd,h)->rva),iv) ) {
                int aux = h;
                h = bdd_low(bdd,h);
                if ( aux == LAST_ADDED(bdd) ) {
                    int subtree  = bdd_high(bdd,aux);
                    if ( aux != l )
                        DELETE_LAST_ELEMENT(bdd);
                    // INCOMPLETE, is this parameter order OK
                    bdd_check_ROBDD(alg,bdd_rt,subtree,iv,l,h,_errmsg);
                }
                if ( h!=0 && h!=1 )
                    aux = h;
                else
                    flag = -1;
            }
        }
    }
    if ( l == h ) return l;
    else {
        int index = bdd_lookup(bdd_rt,iv,l,h);
        if ( index >= 0 )
            return index;
        else 
            return bdd_create_node(bdd_rt,iv,l,h);
    }
} 

/*
 *
 *
 *
 */

static rva RVA_0 = {.var = "0", .val = -1};
static rva RVA_1 = {.var = "1", .val = -1};

static int bdd_start_build(bdd_alg* alg, bdd_runtime* bdd, char** _errmsg) {
#ifdef BDD_VERBOSE
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    if ( bdd->verbose )
        fprintf(stdout,"BDD start_build\n");
#endif
    int len_expr, n_rva, n_spaces;
    if ( !analyze_expr(bdd->core.expr,&len_expr,&n_rva,&n_spaces,_errmsg) )
        return 0;
    // fprintf(stdout,"analyze: len_expr = %d, n_rva = %d, n_spaces = %d\n",len_expr,n_rva,n_spaces);
    bdd->len_expr = len_expr - n_spaces + 1;
    if ( !V_rva_init_estsz(&bdd->order,n_rva) )
        return pg_error(_errmsg,"bdd_start_build: error creating order vector");
    if ( !compute_default_order(&bdd->order,bdd->core.expr,_errmsg))
        return 0;
    bdd->n     = V_rva_size(&bdd->order);
    if ( !V_rva_node_init_estsz(&bdd->core.tree, bdd->n+2) )
        return pg_error(_errmsg,"bdd_start_build: error creating tree");
#ifdef BDD_VERBOSE
    bdd->mk_calls = 0;
    if ( bdd->verbose ) {
        fprintf(stdout,"/-------START, alg=\"%s\"--------\n",alg->name);
        bdd_print(bdd,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
#endif
    char* rewrite_buffer = (char*)MALLOC(((bdd->n+1) * bdd->len_expr));
    // the last buffer is for the expression self
    char* exprbuff = rewrite_buffer + (bdd->n*bdd->len_expr);
    bdd_strcpy_nospaces(exprbuff,bdd->core.expr,bdd->len_expr);
    // memcpy(exprbuff,bdd->core.expr,bdd->len_expr);
    // Remove all spaces from the buffer! This will result in final expressions
    // with only characters 01&|!(). This way we can build an extremely fast
    // expression evaluator
    //
    if (bdd_create_node(bdd,&RVA_0,BDD_NONE,BDD_NONE)<0) return 0;
    if (bdd_create_node(bdd,&RVA_1,BDD_NONE,BDD_NONE)<0) return 0;
    int res = alg->build(alg,bdd,exprbuff,0,rewrite_buffer,_errmsg);
    if ( (res>=0) && V_rva_node_size(&bdd->core.tree) == 2 ) {
        // there have no nodes been created, expression is constant, no errors
        V_rva_node_reset(&bdd->core.tree);
        if ( res >= 0 ) { // no errors
            if ( res == 0 ) {
                if (bdd_create_node(bdd,&RVA_0,BDD_NONE,BDD_NONE)<0) return 0;
#ifdef SIMPLIFY_CONSTANT
                bdd->core.expr = "0";
#endif
            } else {
                if (bdd_create_node(bdd,&RVA_1,BDD_NONE,BDD_NONE)<0) return 0;
#ifdef SIMPLIFY_CONSTANT
                bdd->core.expr = "1";
#endif
            }
        }
    }
    //
    FREE(rewrite_buffer);
    //
#ifdef BDD_VERBOSE
    if ( bdd->verbose ) {
        fprintf(stdout,"/------FINISH, alg=\"%s\"--------\n",alg->name);
        if ( res < 0 ) 
            fprintf(stdout,"ERROR: res=%d\n",res);
        else
            bdd_print(bdd,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
    pbuff_free(&pbuff_struct);
#endif
    return (res<0) ? 0 : 1;
}

bdd_alg S_BDD_BASE    = {.name = "BASE", .build = bdd_build_bdd_BASE, .mk = bdd_mk_BASE};
bdd_alg *BDD_BASE    = &S_BDD_BASE;

bdd_alg S_BDD_KAJ     = {.name = "KAJ" , .build = bdd_build_bdd_KAJ,  .mk = bdd_mk_BASE};
bdd_alg *BDD_KAJ     = &S_BDD_KAJ;

bdd_alg S_BDD_ROBDD = {.name = "ROBDD", .build = bdd_build_bdd_BASE, .mk = bdd_mk_ROBDD};
bdd_alg *BDD_ROBDD = &S_BDD_ROBDD;

bdd_alg *BDD_DEFAULT = &S_BDD_KAJ;

bdd_alg* bdd_algorithm(char* alg_name, char** _errmsg) {
    if ( (strcmp(alg_name,"base")==0) || (strcmp(alg_name,"default")==0) )
        return BDD_BASE;
    else if ( strcmp(alg_name,"kaj") == 0)
        return BDD_KAJ;
    else if ( strcmp(alg_name,"robdd") == 0)
        return BDD_ROBDD;
    else {
        pg_error(_errmsg,"bdd_algorithm: unknown algorithm: \'%s\'",alg_name);
        return NULL;
    }
}

bdd* create_bdd(bdd_alg* alg, char* expr, char** _errmsg, int verbose) {
    bdd_runtime  bdd_struct;
    bdd_runtime* bdd_rt;

    if ( !(bdd_rt = bdd_init(&bdd_struct,expr,verbose,_errmsg)) )
        return NULL;
    if ( ! bdd_start_build(alg,bdd_rt,_errmsg) ) {
        // something may be wrong because of error, do not free bdd_rt !!!
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

/*
 *
 */

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
        rva_node *row = V_rva_node_getp(tree,i);
        if ( IS_LEAF(row) ) {
            bprintf(pbuff,"\tnode [shape=square]\n");
            generate_label(pbuff,i,&row->rva,(extra ? extra[i] : NULL));
        } else {
            bprintf(pbuff,"\tnode [shape=circle]\n");
            generate_label(pbuff,i,&row->rva,(extra ? extra[i] : NULL));
            bprintf(pbuff,"\tedge [shape=rarrow style=dashed]\n");
            bprintf(pbuff,"\t%d -> %d\n",i,row->low);
            bprintf(pbuff,"\tedge [shape=rarrow style=bold]\n");
            bprintf(pbuff,"\t%d -> %d\n",i,row->high);
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

static int or_length(bdd* bdd, int i) {
    int count = 0;

    rva_node *node = bdd_node(bdd,i);
    while ( ! IS_LEAF(node) ) {
        node = bdd_node(bdd,node->low);
        count++;
    }
    return count;
}

static int and_length(bdd* bdd, int i) {
    int count = 0;

    rva_node *node = bdd_node(bdd,i);
    while ( ! IS_LEAF(node) ) {
        node = bdd_node(bdd,node->high);
        count++;
    }
    return count;
}

static void _bdd2string(pbuff *pb, bdd* bdd, int i) {
    rva_node *node = bdd_node(bdd,i);

    if ( IS_LEAF(node) ) {
        bprintf(pb,"%s",node->rva.var);
        return;
    }
    int ol = or_length(bdd,i);
    int al = and_length(bdd,i);

    if ( (ol == 1) && (al == 1) ) {
        bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
        return;
    }
    if ( ol > al ) {
        // generate '|' chain
        while ( ! IS_LEAF(node) ) {
            if ( IS_LEAF_I(bdd,node->high) )
                bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
            else {
                bprintf(pb,"(");
                _bdd2string(pb, bdd,node->high);
                bprintf(pb,")");
            }
            node = bdd_node(bdd,node->low);
            if ( ! IS_LEAF(node) )
                bprintf(pb," | ");
        }
        return;
    } else {
        // generate '&' chain
        while ( ! IS_LEAF(node) ) {
            if ( IS_LEAF_I(bdd,node->low) )
                bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
            else {
                bprintf(pb,"(");
                _bdd2string(pb,bdd,node->low);
                bprintf(pb,")");
            }
            node = bdd_node(bdd,node->high);
            if ( ! IS_LEAF(node) )
                bprintf(pb," & ");
        }
    }
}

char* bdd2string(bdd* bdd, int encapsulate) {
    pbuff pbuff_struct, *pb=pbuff_init(&pbuff_struct);
    if ( encapsulate ) bprintf(pb,"Bdd(");
    _bdd2string(pb,bdd,V_rva_node_size(&bdd->tree)-1);
    if ( encapsulate ) bprintf(pb,")");
    return pbuff_preserve_or_alloc(pb);
    
}


static double bdd_probability_node(bdd_dictionary* dict, bdd* bdd, int i,char** extra,int verbose,char** _errmsg) {
    rva *rva = bdd_rva(bdd,i);
    // incomplete: highly unefficient, store already computed results
    if (verbose )
        fprintf(stdout,"+ bdd_probability(node=%d,\'%s=%d\')\n",i,rva->var,rva->val);
    double p_root, res;
    rva_node *node = bdd_node(bdd,i);
    if ( IS_LEAF(node) ) {
        // is a '0' or '1' leaf
        res = p_root = (node->rva.var[0] == '0') ? 0.0 : 1.0;
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout,"++ is_leaf: P=%f\n",res);
#endif
    } else {
        p_root = lookup_probability(dict,rva);
        if ( p_root < 0.0 ) {
            pg_error(_errmsg,"dictionary_lookup: rva[\'%s\'] not found.",rva);
            return -1.0;
        }
        int low = node->low;
        int high = node->high;
#define MAURICE_NEW
#ifdef MAURICE_NEW
        while ( IS_SAMEVAR(bdd_rva(bdd,high),rva) ) {
            high = bdd_low(bdd,high);
        }
#endif
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout,"++ is_node(low=%d, high=%d)\n",low,high);
#endif
        double p_l = bdd_probability_node(dict,bdd,low,extra,verbose,_errmsg);
        double p_h = bdd_probability_node(dict,bdd,high,extra,verbose,_errmsg);
        if ( p_l < 0.0 || p_h < 0.0 )
            return -1.0;
        res = (p_root * p_h) + p_l;
        if ( ! IS_SAMEVAR(rva,bdd_rva(bdd,low)) )
            res = res - p_root*p_l;
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout,"+ bdd_probability(node=%d,\'%s=%d\') p_root=%f, P=%f\n",i,rva->var,rva->val,p_root,res);
#endif
   }
   if ( extra )
       sprintf(extra[i],"<i>(%.2f)<br/>%.2f</i>",p_root,res);
   return res;
}

double bdd_probability(bdd_dictionary* dict, bdd* bdd,char** extra, int verbose, char** _errmsg) {
    int topnode = V_rva_node_size(&bdd->tree) - 1;
    
    return bdd_probability_node(dict,bdd,topnode,extra,verbose,_errmsg);
}
