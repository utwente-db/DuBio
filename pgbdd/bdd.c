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

#include <limits.h>
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

static rva RVA_0 = {.var = "0", .val = -1};
static rva RVA_1 = {.var = "1", .val = -1};

static bdd_runtime* bdd_rt_init(bdd_runtime* bdd_rt, char* expr, int verbose, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( verbose ) 
        fprintf(stdout,"Create bdd: %s\n",expr);
    bdd_rt->verbose     = verbose;
    bdd_rt->mk_calls    = 0;
    bdd_rt->check_calls = 0;
#endif
#ifdef BDD_STORE_EXPRESSION
    bdd_rt->core.expr   = expr;
#endif
    bdd_rt->expr        = expr;
    bdd_rt->G_cache     = NULL;
    return bdd_rt;
}

void bdd_rt_free(bdd_runtime* bdd_rt) {
    V_rva_free(&bdd_rt->order);
    V_rva_node_free(&bdd_rt->core.tree);
    if ( bdd_rt->G_cache )
        FREE(bdd_rt->G_cache);
}

static void bdd_print_row(bdd* par_bdd, nodei i, pbuff* pbuff) {
    rva_node* row = BDD_NODE(par_bdd,i);
    if ( row->rva.val < 0 )
        bprintf(pbuff,"[\"%s\"]",row->rva.var);
    else
        bprintf(pbuff,"[\"%s=%d\",%d,%d]",row->rva.var,row->rva.val,row->low,row->high);
}

static void bdd_print_tree(bdd* par_bdd, pbuff* pbuff) {
    for(nodei i=0; i<BDD_TREESIZE(par_bdd); i++) {
        bprintf(pbuff,"\t%d:\t",i);
        bdd_print_row(par_bdd,i,pbuff);
        bprintf(pbuff,"\n");
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

static void bdd_print(bdd_runtime* bdd_rt, pbuff* pbuff) {
    bprintf(pbuff,"+ expr     = %s\n",bdd_rt->expr);
    bprintf(pbuff,"+ order    = ");
    bdd_print_V_rva(&bdd_rt->order,pbuff);
    bprintf(pbuff,"\n");
    bprintf(pbuff,"N          = %d\n",bdd_rt->n);
    bprintf(pbuff,"mk_calls   = %d\n",bdd_rt->mk_calls);
    bprintf(pbuff,"check_calls= %d\n",bdd_rt->check_calls);
    bprintf(pbuff,"Tree       = [\n");
    bdd_print_tree(&bdd_rt->core,pbuff);
    bprintf(pbuff,"]\n");
}
#endif

void bdd_info(bdd* bdd, pbuff* pbuff) {
#ifdef BDD_STORE_EXPRESSION
    bprintf(pbuff,"%s\n\n",bdd->expr);
#else
    bdd2string(pbuff,bdd,0);
    bprintf(pbuff,"\n\n");
#endif
    bdd_print_tree(bdd,pbuff);
}

static nodei bdd_lookup(bdd_runtime* bdd_rt, rva* rva, nodei low, nodei high) {
    // ORIGINAL: 
    // rva_node tofind = { .rva  = *rva, .low  = low, .high = high };
    // return V_rva_node_find(&bdd_rt->core.tree,cmpRva_node,&tofind);
    // OPTIMIZED:
    V_rva_node *tree = &bdd_rt->core.tree;
    for (nodei i=0; i<tree->size; i++) {
        rva_node* n = &tree->items[i];
        if ( (n->low == low) && 
             (n->high == high) &&
             (n->rva.val = rva->val) &&
             (strcmp((char*)&n->rva.var,(char*)&rva->var)==0)) {
            return i;
        }
    }
    return NODEI_NONE;
} 

static nodei bdd_create_node(bdd* bdd, rva* rva, nodei low, nodei high) {
    rva_node newrow = { .rva  = *rva, .low  = low, .high = high };

    if ( (BDD_TREESIZE(bdd) + 1) >= NODEI_MAX )
        return NODEI_NONE; // error, end of 'nodei' range
    int new_index = V_rva_node_add(&bdd->tree,&newrow);
    return (new_index < 0) ? NODEI_NONE : (nodei)new_index;
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
    if ( (res->val = bdd_atoi(val)) == NODEI_NONE )
        return pg_error(_errmsg,"bad rva string, bad value %s= %s",res->var,val);   
    return 1;
}

static int add_to_order(V_rva* order, rva* new_rva) {
// #define KEEP_ORDER_SORTED
#ifdef KEEP_ORDER_SORTED
    int index = V_rva_bsearch(order,cmpRva,new_rva);
    if ( index < 0 ) {
        index = -(index + VECTOR_BSEARCH_NEG_OFFSET);
        return (V_rva_insert_at(order,index,new_rva) < 0) ? BDD_FAIL : BDD_OK;
    } else
        return 1; // OK, already in order
#else
    return (V_rva_add(order,new_rva) < 0) ? BDD_FAIL : BDD_OK;
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
                    return BDD_FAIL;
                if ( !add_to_order(order,&new_rva) )
                    return BDD_FAIL;
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

static nodei bdd_mk_BASE(bdd_runtime* bdd_rt, rva *v, nodei l, nodei h, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        for(int i=0;i<bdd_rt->call_depth; i++)
            fprintf(stdout,">>");
        fprintf(stdout," MK{BASE}[v=\"%s=%d\", l=%d, h=%d]\n",v->var,v->val,l,h);
    }
    bdd_rt->mk_calls++;
#endif
    if ( l == h )
        return h;
    nodei node = bdd_lookup(bdd_rt,v,l,h);
    if ( node != NODEI_NONE ) 
        return node; /* node already exists */ 
    nodei res = bdd_create_node(&bdd_rt->core,v,l,h);
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        for(int i=0;i<bdd_rt->call_depth; i++)
            fprintf(stdout,">>");
        fprintf(stdout," CREATED NODE[%d](v=\"%s=%d\", l=%d, h=%d)\n",res,v->var,v->val,l,h);
    }
#endif
    return res;
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

static nodei bdd_build_bdd_BASE(bdd_alg* alg, bdd_runtime* bdd_rt, char* expr, int i, char* rewrite_buffer, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose )
        fprintf(stdout,"BUILD{%s}[i=%d]: %s\n",alg->name,i,expr);
#endif
    if ( i >= bdd_rt->n ) {
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose )
            fprintf(stdout,"EVAL{%s}[i=%d]: %s = ",alg->name,i,expr);
#endif
        int res = bee_eval(expr,_errmsg);
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose )
            fprintf(stdout,"%d\n",res);
#endif
        return res;
    }
    rva* var = V_rva_getp(&bdd_rt->order,i);
    char* newexpr = rewrite_buffer + (i*bdd_rt->len_expr); // Pretty brill:-)
    char rva_string[MAX_RVA_LEN];
    create_rva_string(rva_string,var);
    bdd_replace_str(newexpr,expr,rva_string,'0');
    nodei l = alg->build(alg,bdd_rt,newexpr,i+1,rewrite_buffer,_errmsg);
    bdd_replace_str(newexpr,expr,rva_string,'1');
    nodei h = alg->build(alg,bdd_rt,newexpr,i+1,rewrite_buffer,_errmsg);
    if ( l == NODEI_NONE || h == NODEI_NONE )
        return NODEI_NONE;
    else
        return alg->mk(bdd_rt,var,l,h,_errmsg);
}

/*
 * Kaj's algorithm
 */

static nodei bdd_build_bdd_KAJ(bdd_alg* alg, bdd_runtime* bdd_rt, char* expr, int order_i, char* rewrite_buffer, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose )
        fprintf(stdout,"BUILD{%s}[i=%d]: %s\n",alg->name,order_i,expr);
#endif
    if ( order_i >= bdd_rt->n ) {
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose )
            fprintf(stdout,"EVAL{%s}[i=%d]: %s = ",alg->name,order_i,expr);
#endif
        int boolean_res = bee_eval(expr,_errmsg);
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose )
            fprintf(stdout,"%d\n",boolean_res);
#endif
        return (boolean_res < 0) ? NODEI_NONE : (nodei)boolean_res;
    }
    rva* var = V_rva_getp(&bdd_rt->order,order_i);
    char* newexpr = rewrite_buffer + (order_i*bdd_rt->len_expr); // Pretty brill:-)
    char rva_string[MAX_RVA_LEN];
    create_rva_string(rva_string,var);
    bdd_replace_str(newexpr,expr,rva_string,'0');
    nodei l = alg->build(alg,bdd_rt,newexpr,order_i+1,rewrite_buffer,_errmsg);
    if ( l == NODEI_NONE ) return NODEI_NONE;
    bdd_replace_str(newexpr,expr,rva_string,'1');
    //
    int scan_samevar_i = order_i+1;
    while ( scan_samevar_i < V_rva_size(&bdd_rt->order) ) {
         rva* next_in_order = V_rva_getp(&bdd_rt->order,scan_samevar_i);
         if ( !IS_SAMEVAR(var,next_in_order) )
             break; // varname changed, continue build from this index
         if ( var->val != next_in_order->val ) {
             char to_be_zerod[MAX_RVA_LEN];
             create_rva_string(to_be_zerod,next_in_order);
             memcpy(expr,newexpr,bdd_rt->len_expr);
             bdd_replace_str(newexpr,expr,to_be_zerod,'0');
         }
         scan_samevar_i++;
    }
    nodei h = alg->build(alg,bdd_rt,newexpr,scan_samevar_i/*newvar*/,rewrite_buffer,_errmsg);
    if ( h == NODEI_NONE )
        return NODEI_NONE;
    else
        return alg->mk(bdd_rt,var,l,h,_errmsg);
}

/*
 * The ROBDD algorithms
 *
 * INCOMPLETE: alg is wrong!!! "(z=1)&!((x=1)&((y=1|y=2)&x=2))" = 0 <> "z=1"
 *
 */

static void bdd_check_ROBDD(bdd_runtime* bdd_rt, nodei root, rva* iv, nodei l, nodei h, char** _errmsg) {
    bdd* bdd = &bdd_rt->core;
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose )
        fprintf(stdout,"CHECK{ROBDD}[root=%d, l=%d, h=%d]\n",root,l,h);
    bdd_rt->check_calls++;
#endif
    nodei low_branch  = 1;
    nodei high_branch = 1;
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
            bdd_check_ROBDD(bdd_rt,high_branch,iv,l,h,_errmsg);
            bdd_check_ROBDD(bdd_rt, low_branch,iv,l,h,_errmsg);
        } else {
            bdd_check_ROBDD(bdd_rt, low_branch,iv,l,h,_errmsg);
            bdd_check_ROBDD(bdd_rt,high_branch,iv,l,h,_errmsg);
        }
    }
}

static nodei bdd_mk_ROBDD(bdd_runtime* bdd_rt, rva *iv, nodei l, nodei h, char** _errmsg) {
    bdd* bdd = &bdd_rt->core;
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose )
        fprintf(stdout,"MK{ROBDD}[v=\"%s=%d\", l=%d, h=%d]\n",iv->var,iv->val,l,h);
    bdd_rt->mk_calls++;
#endif
    int flag = 0;
    if ( l == h ) return l;
    else {
        nodei index = bdd_lookup(bdd_rt,iv,l,h);
        if ( index != NODEI_NONE ) 
            return index;
        if ( h != 0 && h != 1 && flag == 0) {
            // INCOMPLETE: error h may be too big
            if ( h >= bdd->tree.size ) {
                pg_fatal("HERE");
            }
            while ( (h<bdd->tree.size) && IS_SAMEVAR(BDD_RVA(bdd,h),iv) ) {
                nodei aux = h;
                h = bdd_low(bdd,h);
                if ( aux == LAST_ADDED(bdd) ) {
                    nodei subtree  = bdd_high(bdd,aux);
                    if ( aux != l )
                        DELETE_LAST_ELEMENT(bdd);
                    // INCOMPLETE, is this parameter order OK
                    bdd_check_ROBDD(bdd_rt,subtree,iv,l,h,_errmsg);
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
        nodei index = bdd_lookup(bdd_rt,iv,l,h);
        if ( index != NODEI_NONE )
            return index;
        else 
            return bdd_create_node(&bdd_rt->core,iv,l,h);
    }
} 

/*
 *
 *
 *
 */

static int bdd_start_build(bdd_alg* alg, bdd_runtime* bdd_rt, char** _errmsg) {
#ifdef BDD_VERBOSE
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    if ( bdd_rt->verbose )
        fprintf(stdout,"BDD start_build\n");
#endif
    int len_expr, n_rva, n_spaces;
    if ( !analyze_expr(bdd_rt->expr,&len_expr,&n_rva,&n_spaces,_errmsg) )
        return BDD_FAIL;
    // fprintf(stdout,"analyze: len_expr = %d, n_rva = %d, n_spaces = %d\n",len_expr,n_rva,n_spaces);
    bdd_rt->len_expr = len_expr - n_spaces + 1;
    if ( !V_rva_init_estsz(&bdd_rt->order,n_rva) )
        return pg_error(_errmsg,"bdd_rt: error creating order vector");
    if ( !compute_default_order(&bdd_rt->order,bdd_rt->expr,_errmsg))
        return BDD_FAIL;
    bdd_rt->n     = V_rva_size(&bdd_rt->order);
    if ( !V_rva_node_init_estsz(&bdd_rt->core.tree, bdd_rt->n+2) )
        return pg_error(_errmsg,"bdd_rt: error creating tree");
#ifdef BDD_VERBOSE
    bdd_rt->mk_calls = 0;
    if ( bdd_rt->verbose ) {
        fprintf(stdout,"/-------START, alg=\"%s\"--------\n",alg->name);
        bdd_print(bdd_rt,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
#endif
    char* rewrite_buffer = (char*)MALLOC(((bdd_rt->n+1) * bdd_rt->len_expr));
    // the last buffer is for the expression self
    char* exprbuff = rewrite_buffer + (bdd_rt->n*bdd_rt->len_expr);
    bdd_strcpy_nospaces(exprbuff,bdd_rt->expr,bdd_rt->len_expr);
    // memcpy(exprbuff,bdd_rt->core.expr,bdd_rt->len_expr);
    // Remove all spaces from the buffer! This will result in final expressions
    // with only characters 01&|!(). This way we can build an extremely fast
    // expression evaluator
    //
    if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE)==NODEI_NONE) return BDD_FAIL;
    if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE)==NODEI_NONE) return BDD_FAIL;
    nodei res = alg->build(alg,bdd_rt,exprbuff,0,rewrite_buffer,_errmsg);
    if ( (res != NODEI_NONE) && V_rva_node_size(&bdd_rt->core.tree) == 2 ) {
        // there have no nodes been created, expression is constant, no errors
        V_rva_node_reset(&bdd_rt->core.tree);
        if ( res == 0 ) {
            if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
#ifdef BDD_STORE_EXPRESSION
            bdd_rt->core.expr = "0";
#endif
        } else {
            if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
#ifdef BDD_STORE_EXPRESSION
            bdd_rt->core.expr = "1";
#endif
        }
    }
    //
    FREE(rewrite_buffer);
    //
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        fprintf(stdout,"/------FINISH, alg=\"%s\"--------\n",alg->name);
        if ( res == NODEI_NONE ) 
            fprintf(stdout,"ERROR: res=%d\n",res);
        else
            bdd_print(bdd_rt,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
    pbuff_free(&pbuff_struct);
#endif
    return (res == NODEI_NONE) ? BDD_FAIL : BDD_OK;
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

    if ( !(bdd_rt = bdd_rt_init(&bdd_struct,expr,verbose,_errmsg)) )
        return NULL;
    if ( ! bdd_start_build(alg,bdd_rt,_errmsg) ) {
        // something may be wrong because of error, do not free bdd_rt !!!
        return NULL;
    }
    bdd* res = serialize_bdd(&bdd_rt->core);
    bdd_rt_free(bdd_rt);

    return res;
}

/*
 *
 */

#define BDD_G_CACHE_POSSIBLE(L,R) (((L+1)*(R+1)*sizeof(short)) < BDD_G_CACHE_MAX)

#define G_INDEX(BRTP,L,R)         ((L)*((int)(BRTP)->G_l) + (R))


static int init_G(bdd_runtime* bdd_rt,int l_root, int r_root, char** _errmsg) {
    if ( ! BDD_G_CACHE_POSSIBLE(l_root,r_root) )
        return pg_error(_errmsg,"bdd_operation:init_G:apply too complex: (%d x %d x %d) > %d",l_root+1,r_root+1,sizeof(short),BDD_G_CACHE_MAX);
    bdd_rt->G_l = (unsigned short)l_root+1;
    bdd_rt->G_r = (unsigned short)r_root+1;
    size_t sz = bdd_rt->G_l * bdd_rt->G_r * sizeof(short); 
    if ( !(bdd_rt->G_cache = (unsigned short*)MALLOC(sz)) )
        return BDD_FAIL;
    memset(bdd_rt->G_cache,0,sz);
    return 1;
}

#define G_INDEX(BRTP,L,R)       ((L)*((int)(BRTP)->G_l) + (R))

static void store_G(bdd_runtime* bdd_rt,int l, int r, int v) {
    bdd_rt->G_cache[G_INDEX(bdd_rt,l,r)] = (unsigned short)(v+1);
}

static int lookup_G(bdd_runtime* bdd_rt, int l, int r) {
    int res = (int)bdd_rt->G_cache[G_INDEX(bdd_rt,l,r)] - 1;
#ifdef BDD_VERBOSE
    if ( res >= 0 )
        bdd_rt->G_cache_hits++;
#endif
    return res;
}

static nodei _bdd_apply(bdd_runtime* bdd_rt, char op, bdd* b1, nodei u1, bdd* b2, nodei u2, char** _errmsg)
{
    nodei u;

#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        bdd_rt->call_depth++;
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        for(int i=0;i<bdd_rt->call_depth; i++)
            bprintf(pbuff,">>");
        bprintf(pbuff," _bdd_apply(");
        bdd_print_row(b1,u1,pbuff);
        bprintf(pbuff," , ");
        bdd_print_row(b2,u2,pbuff);
        bprintf(pbuff,")\n");
        pbuff_flush(pbuff,stdout);
    }
#endif
    if ( (u = lookup_G(bdd_rt,u1,u2)) < 0 ) {
        rva_node *n_u1 = BDD_NODE(b1,u1);
        rva_node *n_u2 = BDD_NODE(b2,u2);
        if ( IS_LEAF(n_u1) && IS_LEAF(n_u2) ) {
            nodei bv_u1 = LEAF_BOOLVALUE(n_u1);
            nodei bv_u2 = LEAF_BOOLVALUE(n_u2);
            u = (op=='&') ? (bv_u1 & bv_u2) : (bv_u1 | bv_u2);
        } else {
            int cmp = cmpRva(&n_u1->rva,&n_u2->rva);
            if ( cmp == 0 ) {
                u = bdd_mk_BASE(bdd_rt, &n_u1->rva,
                        _bdd_apply(bdd_rt,op,b1,n_u1->low,b2,n_u2->low,_errmsg),
                        _bdd_apply(bdd_rt,op,b1,n_u1->high,b2,n_u2->high,_errmsg),
                        _errmsg);
            } else if ( IS_LEAF(n_u1) || (cmp < 0) ) {
                u = bdd_mk_BASE(bdd_rt, &n_u2->rva,
                        _bdd_apply(bdd_rt,op,b1,u1,b2,n_u2->low,_errmsg),
                        _bdd_apply(bdd_rt,op,b1,u1,b2,n_u2->high,_errmsg),
                        _errmsg);
            } else { /* IS_LEAF(n_u2) || (cmp > 0) */
                u = bdd_mk_BASE(bdd_rt, &n_u1->rva,
                        _bdd_apply(bdd_rt,op,b1,n_u1->low, b2,u2,_errmsg),
                        _bdd_apply(bdd_rt,op,b1,n_u1->high,b2,u2,_errmsg),
                        _errmsg);
            }
        }
        store_G(bdd_rt,u1,u2,u);
    }
#ifdef BDD_VERBOSE
    bdd_rt->call_depth--;
#endif
    return u;
}

bdd* bdd_apply(char op,bdd* b1,bdd* b2,int verbose, char** _errmsg) {
    bdd_runtime bdd_rt_struct, *bdd_rt;
    if ( !(bdd_rt = bdd_rt_init(&bdd_rt_struct,"",verbose/*verbose*/,_errmsg)) )
        return NULL;
    V_rva_node_init(&bdd_rt->core.tree); // Ugly, should have been done in _init
    VECTOR_EMPTY(&bdd_rt->order);        // handy macro
    if (!init_G(bdd_rt,BDD_ROOT(b1),BDD_ROOT(b2),_errmsg))
        return NULL;
    //
    if ( (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE)==NODEI_NONE) ||
         (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE)==NODEI_NONE) ) {
        pg_error(_errmsg,"bdd_apply: tree init [0,1] fails");
        return NULL;
    }
#ifdef BDD_VERBOSE
    bdd_rt->call_depth   = 0;
    bdd_rt->G_cache_hits = 0;
#endif
    if (_bdd_apply(bdd_rt,op,b1,BDD_ROOT(b1),b2,BDD_ROOT(b2),_errmsg) == NODEI_NONE)
        return NULL;
    bdd* res = serialize_bdd(&bdd_rt->core);
    bdd_rt_free(bdd_rt);
    //
    return res;
}

/*
 *
 */

static bdd* _bdd_not(bdd* par_bdd, char** _errmsg) {
    /* bdd ! operation. Could be even faster by switching node 0 and 1. But 
     * I think is is safer to have '0' at 0 and '1' at 1 so I only switch the
     * pointers in the tree to these LEAF nodes.
     */
    bdd* res = serialize_bdd(par_bdd);
    nodei root = BDD_ROOT(res);
    if ( root == 0 ) { // just one element '0' or '1'
        rva_node *node = BDD_NODE(res,root);
        node->rva.var[0] = (node->rva.var[0] == '0') ? '1' : '0';
    } else {
        for(nodei i=0 /*?2?*/; i<V_rva_node_size(&res->tree); i++) {
            rva_node *node = BDD_NODE(res,i);
            
            if ( !IS_LEAF(node) ) { // check not necessary when starting at 2
                if ( node->low <2 ) node->low = (node->low ==1) ? 0 : 1;
                if ( node->high<2 ) node->high= (node->high==1) ? 0 : 1;
            }
        }
    }
    return res;
}

static bdd* _bdd_binary_op_by_text(char operator, bdd* lhs_bdd, bdd* rhs_bdd, char** _errmsg) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);

    bprintf(pbuff,"(");
    bdd2string(pbuff,lhs_bdd,0);
    bprintf(pbuff,")%c(",operator);
    bdd2string(pbuff,rhs_bdd,0);
    bprintf(pbuff,")");
    bdd* res = create_bdd(BDD_DEFAULT,pbuff->buffer,_errmsg,0/*verbose*/);
    pbuff_free(pbuff);
    //
    return res;
}

bdd* bdd_operator(char operator, op_mode m, bdd* lhs, bdd* rhs, char** _errmsg) {
    if ( !lhs ) {
         pg_error(_errmsg,"_bdd_operator: lhs bdd NULL");
         return NULL;
    }
    if ( operator == '!' ) 
        return _bdd_not(lhs,_errmsg); // ignore m BY_TEXT
    else  if ( operator == '|' || operator == '&' ) {
        if ( !rhs ) {
             pg_error(_errmsg,"_bdd_operator: rhs bdd NULL");
             return NULL;
        }
        if ( m == BY_APPLY ) {
            if ( BDD_G_CACHE_POSSIBLE(BDD_ROOT(lhs),BDD_ROOT(rhs)) )
                return bdd_apply(operator,lhs,rhs,0,_errmsg);
        } // else  
            // cache for apply would be too big, text variant is less complex ???
        return _bdd_binary_op_by_text(operator,lhs,rhs,_errmsg);
    } else {
             pg_error(_errmsg,"_bdd_operator: bad operator (%c)",operator);
             return NULL;
    }
}

#define BDD_BASE_SIZE   (sizeof(bdd) - sizeof(V_rva_node))

bdd* serialize_bdd(bdd* tbs) {
    int bytesize, tree_size;

    V_rva_node_shrink2size(&tbs->tree);
    tree_size= V_rva_node_bytesize(&tbs->tree);
    bytesize = BDD_BASE_SIZE + tree_size;
#ifdef BDD_STORE_EXPRESSION
    int expr_size  = strlen(tbs->expr) + 1;
    bytesize  += expr_size;
#endif
    bdd* res = NULL;
    if ( (res = (bdd*)MALLOC(bytesize)) ) {
        res->bytesize = bytesize;
        V_rva_node_serialize(&res->tree,&tbs->tree);
#ifdef BDD_STORE_EXPRESSION
        res->expr     = memcpy(((char*)&res->tree)+tree_size,tbs->expr,expr_size);
#endif
        return res;
    } 
    else
        return NULL; // incomplete, errmsg here
}

bdd* relocate_bdd(bdd* tbr) {
    V_rva_node_relocate(&tbr->tree);
#ifdef BDD_STORE_EXPRESSION
    tbr->expr     = ((char*)&tbr->tree)+V_rva_node_bytesize(&tbr->tree);
#endif
    return tbr;
}

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
#ifdef BDD_STORE_EXPRESSION
    bprintf(pbuff,"\tlabel=\"bdd(\'%s\')\";\n",bdd->expr);
#else
    bprintf(pbuff,"\tlabel=\"bdd(\'");
    bdd2string(pbuff,bdd,0);
    bprintf(pbuff,"\')\";\n");
#endif
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

    rva_node *node = BDD_NODE(bdd,i);
    while ( ! IS_LEAF(node) ) {
        node = BDD_NODE(bdd,node->low);
        count++;
    }
    return count;
}

static int and_length(bdd* bdd, int i) {
    int count = 0;

    rva_node *node = BDD_NODE(bdd,i);
    while ( ! IS_LEAF(node) ) {
        node = BDD_NODE(bdd,node->high);
        count++;
    }
    return count;
}

static void _bdd2string(pbuff *pb, bdd* bdd, nodei i) {
    rva_node *node = BDD_NODE(bdd,i);

    if ( IS_LEAF(node) ) {
        bprintf(pb,"%s",node->rva.var);
        return;
    }
    int ol = or_length(bdd,i);
    int al = and_length(bdd,i);

    if ( (ol == 1) && (al == 1) ) {
        if ( node->low == 1 ) // negated
            bprintf(pb,"!");
        bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
        return;
    }
    int closepar = 0;
    if ( ol > al ) {
        // generate '|' chain
        if ( IS_LEAF_I(bdd,node->high) && node->high == 0 ) {
            bprintf(pb,"!("); // is a not chain
            closepar = 1;
        }
        while ( ! IS_LEAF(node) ) {
            if ( IS_LEAF_I(bdd,node->high) ) {
                bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
            } else {
                bprintf(pb,"(");
                bprintf(pb,"%s=%d & ",node->rva.var,node->rva.val);
                _bdd2string(pb, bdd,node->high);
                bprintf(pb,")");
            }
            node = BDD_NODE(bdd,node->low);
            if ( ! IS_LEAF(node) )
                bprintf(pb," | ");
        }
        if ( closepar )
            bprintf(pb,")");
        return;
    } else {
        // generate '&' chain
        if ( IS_LEAF_I(bdd,node->low) && node->low == 1 ) {
            bprintf(pb,"!("); // is a not chain
            closepar = 1;
        }
        while ( ! IS_LEAF(node) ) {
            if ( IS_LEAF_I(bdd,node->low) ) {
                bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
            } else {
                bprintf(pb,"(");
                bprintf(pb,"%s=%d | ",node->rva.var,node->rva.val);
                _bdd2string(pb,bdd,node->low);
                bprintf(pb,")");
            }
            node = BDD_NODE(bdd,node->high);
            if ( ! IS_LEAF(node) )
                bprintf(pb," & ");
        }
        if ( closepar )
            bprintf(pb,")");
    }
}

void bdd2string(pbuff* pb, bdd* bdd, int encapsulate) {
    if ( encapsulate ) bprintf(pb,"Bdd(");
    _bdd2string(pb,bdd,BDD_ROOT(bdd));
    if ( encapsulate ) bprintf(pb,")");
}


static double bdd_probability_node(bdd_dictionary* dict, bdd* bdd, nodei i,char** extra,int verbose,char** _errmsg) {
    rva *rva = BDD_RVA(bdd,i);
    // incomplete: highly unefficient, store already computed results
    if (verbose )
        fprintf(stdout,"+ bdd_probability(node=%d,\'%s=%d\')\n",i,rva->var,rva->val);
    double p_root, res;
    rva_node *node = BDD_NODE(bdd,i);
    if ( IS_LEAF(node) ) {
        // is a '0' or '1' leaf
        res = p_root = LEAF_BOOLVALUE(node) ? 1.0 : 0.0;
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
        nodei low = node->low;
        nodei high = node->high;
#define MAURICE_NEW
#ifdef MAURICE_NEW
        while ( IS_SAMEVAR(BDD_RVA(bdd,high),rva) ) {
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
        if ( ! IS_SAMEVAR(rva,BDD_RVA(bdd,low)) )
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
    return bdd_probability_node(dict,bdd,BDD_ROOT(bdd),extra,verbose,_errmsg);
}

static int _contains_rva(bdd* bdd, char* var, int val) {
    for (int i=0; i<V_rva_node_size(&bdd->tree); i++) {
        rva_node* node = BDD_NODE(bdd,i);
        if ( !IS_LEAF(node)) {
            if ( strcmp(var,node->rva.var) == 0) {
                 if ( (val < 0) || (val == node->rva.val) )
                     return 1;
            }
        }
    }
    return 0;
}

int bdd_property_check(bdd* bdd, int prop, char* s, char** _errmsg) {
    if ( !(IS_VALID_PROPERTY(prop)) ) {
        pg_error(_errmsg,"bdd_property_check: bad property (%d)",prop);
        return -1;
    } if ( prop==BDD_IS_FALSE || prop==BDD_IS_TRUE ) {
        if ( V_rva_node_size(&bdd->tree) == 1 ) {
            rva_node* node = BDD_NODE(bdd,0);
            return LEAF_BOOLVALUE(node)==prop; // :-)   
        }
    } else if ( prop==BDD_HAS_VARIABLE ) {
        return _contains_rva(bdd,s,-1);
    } else if ( prop==BDD_HAS_RVA ) {
        char *varp, *eov, *valp;
        varp = s;
        while ( isspace(*varp) ) varp++;
        if ( *varp ) {
            valp = varp;
            while ( isalnum(*valp) ) valp++;
            if ( *valp ) {
                eov = valp;
                while ( *valp && *valp != '=') valp++;
                if ( *valp == '=') {
                    *eov = 0;
                    int val = bdd_atoi(++valp);
                    if ( val != NODEI_NONE) {
                        return _contains_rva(bdd,varp,val);
                    }
                }
            }
        }
        pg_error(_errmsg,"bdd_property_check: bad rva (%s)",s);
        return -1;
    }
    return 0;
}
