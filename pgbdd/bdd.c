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

/* 
 * TODO:
 * + equiv() als Postgres op
 * equiv() met random gen gebruiken exotische equiv
 * equiv() regeneration probleem oplossen.
 * bdd_compare() als C, dan _equal(), _sm(), _gr(), = , < , >
 */

#define BDD_ASSERT

int cmpRva(rva* l, rva* r) {  
    int res = COMPARE_VAR(l->var,r->var);
    if ( res == 0 )
        res = (l->val - r->val);
    return res;
}

DefVectorC(rva_node);

/*
 * int cmpRva_node(rva_node* l, rva_node* r) {
 *     int res = COMPARE_VAR(l->rva.var,r->rva.var);
 *     if ( res == 0 ) {
 *         res = l->rva.var - r->rva.var;
 *         if (res == 0) {
 *             res = (l->low - r->low);
 *             if (res == 0) {
 *                 res = (l->high - r->high);
 *             }
 *         }
 *     }
 *     return res;
 * } 
 */

DefVectorC(rva_order);

int cmpRva_order(rva_order* l, rva_order* r) {  
    int res = COMPARE_VAR(l->rva.var,r->rva.var);
    if ( res == 0 )
        res = (l->rva.val - r->rva.val);
    return res;
}

/*
 *
 *
 */

static rva RVA_0 = {.var = "0", .val = -1};
static rva RVA_1 = {.var = "1", .val = -1};

static int  compute_rva_order(bdd_runtime*, char*,char**);

static bdd_runtime* bdd_rt_init(bdd_runtime* bdd_rt, char* expr, int verbose, char** _errmsg) {
#ifdef BDD_VERBOSE
    bdd_rt->expr        = expr;
    if ( verbose ) 
        fprintf(stdout,"Create bdd: %s\n",(expr?expr:"NULL"));
    bdd_rt->verbose     = verbose;
    bdd_rt->mk_calls    = 0;
    bdd_rt->check_calls = 0;
#endif
    bdd_rt->n           = -1;
    bdd_rt->G_cache     = NULL;
    bdd_rt->rva_epos    = NULL;
    bdd_rt->e_stack     = NULL;
    // 
    if ( expr ) { // apply() does init without expr
        if ( !compute_rva_order(bdd_rt,expr,_errmsg) )
            return NULL;
    }
    if ( !V_rva_node_init(&bdd_rt->core.tree) ) {
        pg_error(_errmsg,"bdd_rt: error creating tree");
        return NULL;
    }
    return bdd_rt;
}

void bdd_rt_free(bdd_runtime* bdd_rt) {
    if ( bdd_rt->G_cache )
        FREE(bdd_rt->G_cache);
    if ( bdd_rt->e_stack )
        FREE(bdd_rt->e_stack);
    if ( bdd_rt->rva_epos )
        FREE(bdd_rt->rva_epos);
    if ( bdd_rt->n >= 0 )
        V_rva_order_free(&bdd_rt->rva_order);
    V_rva_node_free(&bdd_rt->core.tree);
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

static void bdd_print_rva_expr(bdd_runtime* bctx, pbuff* pbuff) {
    bprintf(pbuff,"#Rva & frame expr\n");
    bprintf(pbuff,"-----------------\n");
    bprintf(pbuff,"+ framesz = %d\n",bctx->e_stack_framesz);
    bprintf(pbuff,"+ stack_len = %d\n",bctx->e_stack_len);
    // bprintf(pbuff,"+ [n/c]_rva = %d/%d\n",bctx->n_rva,bctx->c_rva);
    bprintf(pbuff,"+ Sorted/Uniq RVA list (n=%d):\n",bctx->n);
    for (int i=0; i<ORDER_SIZE(bctx); i++) {
        rva_order* rl = ORDER(bctx,i);
        
        bprintf(pbuff,"[%2d]: <%s=%d> pos{",i,rl->rva.var,rl->rva.val);
        locptr p = rl->loc;
        while ( p != LOC_EMPTY ) {
            bprintf(pbuff,"%d",bctx->rva_epos[p].pos);
            p = bctx->rva_epos[p].next;
            if ( p != LOC_EMPTY )
                bprintf(pbuff,",");
        }
        bprintf(pbuff,"}\n");
    }
    bctx->e_stack[bctx->e_stack_len] = 0;
    bprintf(pbuff,"+ baseframe = [%s]\n","INCOMPLETE");
}

static void bdd_print_V_rva_order(V_rva_order* v, pbuff* pbuff) {
    bprintf(pbuff,"{");
    for(int i=0; i<V_rva_order_size(v); i++) {
        rva* rva = &V_rva_order_getp(v,i)->rva;
        bprintf(pbuff,"%s=%d ",rva->var,rva->val);
    }
    bprintf(pbuff,"}");
}

static void bdd_print_frame(bdd_runtime* bctx, int depth, int with_rva, pbuff* pbuff) {
    char* pframe = E_FRAME(bctx,depth);
    
    for(int pos=0; pos<bctx->e_stack_framesz-1; pos++) {
        char c = bee_token2char((bee_token)pframe[pos]);
        if ( c < 0 )
            c = '*'; // ERROR
        if ( ! (c=='0' || c=='1' ) )
            bprintf(pbuff,"%c",c);
        else {
            for (int i=0; i<ORDER_SIZE(bctx); i++) {
                rva_order* rl = ORDER(bctx,i);
                for(locptr p = rl->loc;p!=LOC_EMPTY;p = bctx->rva_epos[p].next){
                    if ( pos == bctx->rva_epos[p].pos ) {
                        if ( with_rva ) {
                            rva* rva = ORDER_RVA(bctx,i);
                            bprintf(pbuff,"<%s=%d>",rva->var,rva->val);
                        }
                        if ( i < depth )
                            bprintf(pbuff,"%c",c);
                        else
                            bprintf(pbuff,"%c",'X');

                    }
                }
            }
        }
    }
}

static nodei bctx_eval_top(bdd_runtime*,int,char**);

static void bdd_reconstruct(bdd_runtime* bctx, int depth) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char* _errmsg;
    char* frame = E_FRAME(bctx,depth);

     
    bprintf(pbuff,"#Reconstruct RVA top expr.\n");
    bprintf(pbuff,"--------------------------\n");
#ifdef BDD_VERBOSE
    bprintf(pbuff,"+ Base expr: %s: \n",bctx->expr);
#endif
    bprintf(pbuff,"+ Topframe : ");
    bdd_print_frame(bctx,depth,1,pbuff);
    bprintf(pbuff,"\n+ Topexpr. : ");
    bdd_print_frame(bctx,depth,0,pbuff);
    bprintf(pbuff," = %d: \n",bctx_eval_top(bctx,depth,&_errmsg));
    for (int i=0; i<ORDER_SIZE(bctx); i++) {
        rva_order* rl = ORDER(bctx,i);
        
        bprintf(pbuff," <%s=%d> = ",rl->rva.var,rl->rva.val);
        bprintf(pbuff,"%d\n",(int)frame[bctx->rva_epos[rl->loc].pos]);
    }
    bprintf(pbuff,"\n");
    pbuff_flush(pbuff, stdout);
    pbuff_free(pbuff);
}

static void bdd_print(bdd_runtime* bdd_rt, pbuff* pbuff) {
    bprintf(pbuff,"+ expr     = %s\n",(bdd_rt->expr?bdd_rt->expr:NULL));
    bprintf(pbuff,"+ order    = ");
    bdd_print_V_rva_order(&bdd_rt->rva_order,pbuff);
    bprintf(pbuff,"\n");
    bprintf(pbuff,"N          = %d\n",bdd_rt->n);
    bprintf(pbuff,"mk_calls   = %d\n",bdd_rt->mk_calls);
    bprintf(pbuff,"check_calls= %d\n",bdd_rt->check_calls);
    if (1) bdd_print_rva_expr(bdd_rt,pbuff);
    bprintf(pbuff,"Tree       = [\n");
    bdd_print_tree(&bdd_rt->core,pbuff);
    bprintf(pbuff,"]\n");
}
#endif

void bdd_info(bdd* bdd, pbuff* pbuff) {
    bdd2string(pbuff,bdd,0);
    bprintf(pbuff,"\n\n");
    bdd_print_tree(bdd,pbuff);
}

static nodei lookup_bdd_node(bdd_runtime* bdd_rt, rva* rva, nodei low, nodei high) {
    // ORIGINAL: 
    // rva_node tofind = { .rva  = *rva, .low  = low, .high = high };
    // return V_rva_node_find(&bdd_rt->core.tree,cmpRva_node,&tofind);
    // OPTIMIZED:
    V_rva_node *tree = &bdd_rt->core.tree;
    for (nodei i=0; i<tree->size; i++) {
        rva_node* n = &tree->items[i];
        if ( (n->low == low) && 
             (n->high == high) &&
             (n->rva.val == rva->val) &&
             (COMPARE_VAR((char*)&n->rva.var,(char*)&rva->var)==0)) {
            return i;
        }
    }
    return NODEI_NONE;
} 

static nodei bdd_create_node(bdd* bdd, rva* rva, nodei low, nodei high) {
    rva_node newrow = { .rva  = *rva, .low  = low, .high = high };
  
    int new_index /*!!no 'nodei'!!*/ = V_rva_node_add(&bdd->tree,&newrow);
    if ( new_index >= NODEI_MAX )
        return NODEI_NONE; // error, end of 'nodei' range
    return (new_index < 0) ? NODEI_NONE : (nodei)new_index;
}

/*
 *
 */

//
//
//
//
//
//
//

static int count_rva(char* p) {
    char c;
    int n_rva = 0;

    while ( (c=*p++) ) {
        if ( c == '=' ) 
            n_rva++;
    }
    return n_rva;
}

static int create_rva_order(rva_order* res, char* var, int var_len, char* val, char** _errmsg) {
    if ( var_len > MAX_RVA_NAME )
        return pg_error(_errmsg,"rva_name too long (max=%d) / %s",MAX_RVA_NAME, var);   
    memcpy(res->rva.var,var,var_len);
    res->rva.var[var_len] = 0;
    if ( strcmp(res->rva.var,"not")==0||strcmp(res->rva.var,"and")==0||strcmp(res->rva.var,"or")==0 )
        return pg_error(_errmsg, "bdd-expr: do not use and/or/not keywords but use '&|!': %s",var);
    if ( (res->rva.val = bdd_atoi(val)) == NODEI_NONE )
        return pg_error(_errmsg,"bad rva value %s= %s",res->rva.var,val);   
    return BDD_OK;
}

static int add2rva_order(bdd_runtime* bctx, rva_order* new_rva) {
    rva_order* rl = NULL;
    int index = V_rva_order_bsearch(&bctx->rva_order,cmpRva_order,new_rva);
    if ( index < 0 ) {
        index = -(index + VECTOR_BSEARCH_NEG_OFFSET);
        if ( (index = V_rva_order_insert_at(&bctx->rva_order,index,new_rva)) < 0)
            return BDD_FAIL;
        rl = ORDER(bctx,index);
        rl->loc = LOC_EMPTY;
    } else {
        rl = ORDER(bctx,index);
    } 
    rva_epos* rp = &(bctx->rva_epos[bctx->c_rva]);
    bctx->e_stack[bctx->e_stack_len] = '1'; // initial test value
    rp->pos  = bctx->e_stack_len++;
    rp->next = rl->loc;
    rl->loc = bctx->c_rva++;
    return BDD_OK;
}

#define ADD2E_STACK(BCTX,C)   (BCTX)->e_stack[(BCTX)->e_stack_len++] = (C)

static int _compute_order(bdd_runtime* bctx, char* expr, char** _errmsg) {
    char *p = expr;
    
    while ( *p ) {
        while ( *p && !isalnum(*p) ) {
            if ( !isspace(*p) )
                ADD2E_STACK(bctx,*p);
            p++;
        }
        if ( isalnum(*p) ) {
            char* start = p;
            //
            if ( isdigit(*p) ) {
                if ( (*p=='0') || (*p=='1') ) {
                    if ( isalnum(p[1]) )
                        return pg_error(_errmsg,"varnames cannot start with a digit: \"%s\"",p);
                    else {
                        ADD2E_STACK(bctx,*p);
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
                rva_order new_rva_order;
                if ( !create_rva_order(&new_rva_order,start,len,p,_errmsg) )
                    return BDD_FAIL;
                if ( !add2rva_order(bctx,&new_rva_order) )
                    return BDD_FAIL;
                while (isdigit(*p) )
                    p++;
            }
        }
    }
    return 1;
}

static int compute_rva_order(bdd_runtime* bctx, char* bdd_expr, char** _errmsg) {
    bctx->e_stack     = (char*)MALLOC(strlen(bdd_expr));
    bctx->e_stack_len = 0; /* during build len grows to determine framesize */
    V_rva_order_init(&bctx->rva_order);
    bctx->n_rva   = count_rva(bdd_expr);
    bctx->c_rva   = 0;
    bctx->rva_epos = (rva_epos*)MALLOC(bctx->n_rva*sizeof(rva_epos));;
    //
    if ( !_compute_order(bctx,bdd_expr,_errmsg) )
        return BDD_FAIL;
    //
    bctx->n                            = ORDER_SIZE(bctx);
    bctx->e_stack[bctx->e_stack_len++] = 0; // terminate the base frame with 0
    bctx->e_stack_framesz              = bctx->e_stack_len;
    bctx->e_stack_len = (bctx->n+1)*bctx->e_stack_framesz; // block 0 is base!
    bctx->e_stack     = (char*)REALLOC(bctx->e_stack,bctx->e_stack_len);
    //
#ifdef BDD_ASSERT
    for (int i=1; i<bctx->n; i++) {
        if ( cmpRva_order(ORDER(bctx,i-1),ORDER(bctx,i)) > 0 )
            pg_fatal("compute_rva_order: order not correctly sorted");
    }
#endif
    if ( bctx->c_rva != bctx->n_rva )
        pg_fatal("bctx_init: assert: rva count unexpected!");
    // now convert the baseframe expression to the 'raw' bee fsm input. 
    if ( parse_tokens(E_FRAME(bctx,0), _errmsg) < 0 )
        return BDD_FAIL;
    // now run 1 'dry' test of the bee evaluator
    if ( bee_eval_raw(E_FRAME(bctx,0),_errmsg) < 0 )
        return BDD_FAIL;
    return 1;
}

/*
 *
 */

static int bctx_orderi_set(bdd_runtime* bctx, int depth, int v_0or1) {
    char* srcframe = E_FRAME(bctx,depth);
    char* dstframe = E_FRAME(bctx,depth+1);
    memcpy(dstframe,srcframe,bctx->e_stack_framesz);
    rva_order* rl = ORDER(bctx,depth);
    for(locptr p = rl->loc; p!=LOC_EMPTY; p = bctx->rva_epos[p].next)
        dstframe[bctx->rva_epos[p].pos] = v_0or1;
    // fprintf(stdout,"**DN[%d,[%s]=%d]: %s\n",depth,rvastr,v_0or1,dstframe);
    return 1;
}

static nodei bctx_eval_top(bdd_runtime* bctx, int depth, char** _errmsg) {
    char* topframe = E_FRAME(bctx,depth);

    int new_res = bee_eval_raw(topframe, _errmsg);
    if ( new_res < 0 )
        return NODEI_NONE; 
    return (nodei)new_res;
}

/*
 * The BASE build()/mk() algorithms impplementing Kaj's algorithm
 */

static nodei bdd_mk(bdd_runtime* bdd_rt, rva *v, nodei l, nodei h, char** _errmsg) {
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        // for(int i=0;i<bdd_rt->call_depth; i++)
        //     fprintf(stdout,">>");
        fprintf(stdout,"MK{BASE}[v=\"%s=%d\", l=%d, h=%d]\n",v->var,v->val,l,h);
    }
    bdd_rt->mk_calls++;
#endif
    if ( l == h )
        return h;
    nodei node = lookup_bdd_node(bdd_rt,v,l,h);
    if ( node != NODEI_NONE ) 
        return node; /* node already exists */ 
    nodei res = bdd_create_node(&bdd_rt->core,v,l,h);
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        // for(int i=0;i<bdd_rt->call_depth; i++)
        //     fprintf(stdout,">>");
        fprintf(stdout,"CREATED NODE[%d](v=\"%s=%d\", l=%d, h=%d)\n",res,v->var,v->val,l,h);
    }
#endif
    return res;
} 

static void _bctx_skip_samevar(bdd_runtime* bctx, int* depth) {
    rva* var = ORDER_RVA(bctx,*depth);
    *depth += 1;
    while ( *depth < bctx->n ) {
         rva* next_in_order = ORDER_RVA(bctx,*depth);
         if ( !IS_SAMEVAR(var,next_in_order) )
             return; // varname changed, continue build from this index
         if ( var->val != next_in_order->val )
             bctx_orderi_set(bctx,*depth,0);
         *depth += 1;
    }
}

static nodei bdd_build(bdd_alg* alg, bdd_runtime* bdd_rt, int depth, char** _errmsg) {
#ifdef BDD_VERBOSE
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);

    if ( bdd_rt->verbose ) {
        bprintf(pbuff,"BUILD{%s}[d=%d]: ",alg->name,depth);
        bdd_print_frame(bdd_rt,depth,1/*no rva*/,pbuff);
        bprintf(pbuff,"\n");
        pbuff_flush(pbuff,stdout);
    }
#endif
    if ( depth >= bdd_rt->n ) {
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose ) {
            bprintf(pbuff,"EVAL{%s}[i=%d]: ",alg->name,depth);
            bdd_print_frame(bdd_rt,depth,0/*no rva*/,pbuff);
            bprintf(pbuff," = ");
            pbuff_flush(pbuff,stdout);
        }
#endif
        int boolean_res = bctx_eval_top(bdd_rt,depth,_errmsg);
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose )
            bprintf(pbuff,"%d\n",boolean_res);
        pbuff_flush(pbuff,stdout);
        pbuff_free(pbuff);
#endif
        return (boolean_res < 0) ? NODEI_NONE : (nodei)boolean_res;
    }
    rva* var = ORDER_RVA(bdd_rt,depth);
    bctx_orderi_set(bdd_rt,depth,0);
    // nodei l = alg->build(alg,bdd_rt,depth+1,_errmsg);
    nodei l = bdd_build(alg,bdd_rt,depth+1,_errmsg);
    if ( l == NODEI_NONE ) return NODEI_NONE;
    bctx_orderi_set(bdd_rt,depth,1);
    _bctx_skip_samevar(bdd_rt,&depth);
    // nodei h = alg->build(alg,bdd_rt,depth,_errmsg);
    nodei h = bdd_build(alg,bdd_rt,depth,_errmsg);
    // return (h == NODEI_NONE) ? NODEI_NONE : alg->mk(bdd_rt,var,l,h,_errmsg);
    return (h == NODEI_NONE) ? NODEI_NONE : bdd_mk(bdd_rt,var,l,h,_errmsg);
}

/*
 * This function checks if two expressions result in two equivalent BDD trees.
 * That is two graphs which, with all '0 and 1' permutations of their rva's
 * have the same truth values.
 *
 * The left must be the largest order size to function correctly
 */
static int _bdd_equiv(int l_depth, bdd_runtime* l_ctx, int r_depth, bdd_runtime* r_ctx, char** _errmsg) {
    //
    // fprintf(stdout,"L=|%s| l_depth=%d\n",l_ctx->expr,l_depth);
    // fprintf(stdout,"R=|%s| r_depth=%d\n",r_ctx->expr,r_depth);
    if ( (l_depth < l_ctx->rva_order.size) || 
         (r_depth < r_ctx->rva_order.size) ) {
        int opt = -1;
        if ( r_depth == r_ctx->rva_order.size )
           opt = 1; // do left, right is end of list
        else if ( l_depth == l_ctx->rva_order.size )
           opt = 2; // do right, left is end of list
        else {
           int cmp = cmpRva_order(ORDER(l_ctx,l_depth),ORDER(r_ctx,r_depth));
           if ( cmp == 0 ) 
               opt = 3; // same rva do both left and right 
           else if (cmp < 0)
               opt = 1; // do left because is first in order
           else
               opt = 2; // do right because is first in order
        }
        if (opt == 1) {
            bctx_orderi_set(l_ctx,l_depth,0);
            int res = _bdd_equiv(l_depth+1,l_ctx,r_depth,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
            bctx_orderi_set(l_ctx,l_depth,1);
            _bctx_skip_samevar(l_ctx,&l_depth);
            res = _bdd_equiv(l_depth,l_ctx,r_depth,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
        } else if (opt==2) {
            bctx_orderi_set(r_ctx,r_depth,0);
            int res = _bdd_equiv(l_depth,l_ctx,r_depth+1,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
            bctx_orderi_set(r_ctx,r_depth,1);
            _bctx_skip_samevar(r_ctx,&r_depth);
            res = _bdd_equiv(l_depth,l_ctx,r_depth,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
        } else { // opt == 3
            bctx_orderi_set(l_ctx,l_depth,0);
            bctx_orderi_set(r_ctx,r_depth,0);
            int res = _bdd_equiv(l_depth+1,l_ctx,r_depth+1,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
            bctx_orderi_set(l_ctx,l_depth,1);
            _bctx_skip_samevar(l_ctx,&l_depth);
            bctx_orderi_set(r_ctx,r_depth,1);
            _bctx_skip_samevar(r_ctx,&r_depth);
            res = _bdd_equiv(l_depth,l_ctx,r_depth,r_ctx, _errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
        }
    } else {
        int l_res = bctx_eval_top(l_ctx,l_depth,_errmsg);
        int r_res = bctx_eval_top(r_ctx,r_depth,_errmsg);
        if ( l_res < 0 || r_res < 0 ) 
            return -1;
        else {
            int res = (l_res==r_res);
#ifdef BDD_VERBOSE
            if ( !res ) {
                bdd_reconstruct(l_ctx,l_depth);
                bdd_reconstruct(r_ctx,r_depth);
            }
#endif
            return res; // outcome of permutation TRUE or FALSE
        }
    }
    return 1;
}
 

/*
 * This function checks if two expressions result in two equivalent BDD trees.
 * That is two graphs which, with all '0 and 1' permutations of their rva's
 * have the same truth values.
 */
int bdd_test_equivalence(char* l_expr, char* r_expr, char** _errmsg) {
    bdd_runtime l_ctx_s;
    bdd_runtime r_ctx_s;

    if ( !bdd_rt_init(&l_ctx_s,l_expr,0,_errmsg) )
        return BDD_FAIL;
    if ( !bdd_rt_init(&r_ctx_s,r_expr,0,_errmsg) )
        return BDD_FAIL;
    int res = _bdd_equiv(0, &l_ctx_s, 0, &r_ctx_s, _errmsg);
    //
    bdd_rt_free(&l_ctx_s);
    bdd_rt_free(&r_ctx_s);
    //
    return res;
}

static int bdd_start_build(bdd_alg* alg, bdd_runtime* bdd_rt, char** _errmsg) {
#ifdef BDD_VERBOSE
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    if ( bdd_rt->verbose )
        fprintf(stdout,"BDD start_build\n");
    bdd_rt->mk_calls = 0;
    if ( bdd_rt->verbose ) {
        fprintf(stdout,"/-------START, alg=\"%s\"--------\n",alg->name);
        bdd_print(bdd_rt,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
#endif
    if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE)==NODEI_NONE) return BDD_FAIL;
    if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE)==NODEI_NONE) return BDD_FAIL;
    nodei res = alg->build(alg,bdd_rt,0,_errmsg);
    if ( (res != NODEI_NONE) && V_rva_node_size(&bdd_rt->core.tree) == 2 ) {
        // there have no nodes been created, expression is constant, no errors
        V_rva_node_reset(&bdd_rt->core.tree);
        if ( res == 0 ) {
            if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
        } else {
            if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
        }
    }
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

bdd_alg S_BDD_BASE    = {.name = "BASE", .build = bdd_build, .mk = bdd_mk};
bdd_alg *BDD_BASE    = &S_BDD_BASE;

bdd_alg *BDD_DEFAULT = &S_BDD_BASE;

bdd_alg* bdd_algorithm(char* alg_name, char** _errmsg) {
    if ( (strcmp(alg_name,"base")==0) || (strcmp(alg_name,"default")==0) )
        return BDD_BASE;
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

#define BDD_BASE_SIZE   (sizeof(bdd) - sizeof(V_rva_node))

bdd* serialize_bdd(bdd* tbs) {
    int bytesize, tree_size;

    V_rva_node_shrink2size(&tbs->tree);
    tree_size= V_rva_node_bytesize(&tbs->tree);
    bytesize = BDD_BASE_SIZE + tree_size;
    bdd* res = NULL;
    if ( (res = (bdd*)MALLOC(bytesize)) ) {
        res->bytesize = bytesize;
        V_rva_node_serialize(&res->tree,&tbs->tree);
        return res;
    } 
    else
        return NULL; // incomplete, errmsg here
}

bdd* relocate_bdd(bdd* tbr) {
    V_rva_node_relocate(&tbr->tree);
    return tbr;
}

/*
 * BDD apply() and &,|,! operator section
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
        // for(int i=0;i<bdd_rt->call_depth; i++)
        //     bprintf(pbuff,">>");
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
            int bv_u1 = LEAF_BOOLVALUE(n_u1);
            int bv_u2 = LEAF_BOOLVALUE(n_u2);
            u = (op=='&') ? (bv_u1 & bv_u2) : (bv_u1 | bv_u2);
        } else {
            int cmp = cmpRva(&n_u1->rva,&n_u2->rva);
            if ( cmp == 0 ) {
                u = bdd_mk(bdd_rt, &n_u1->rva,
                        _bdd_apply(bdd_rt,op,b1,n_u1->low,b2,n_u2->low,_errmsg),
                        _bdd_apply(bdd_rt,op,b1,n_u1->high,b2,n_u2->high,_errmsg),
                        _errmsg);
            } else if ( IS_LEAF(n_u1) || (cmp < 0) ) {
                u = bdd_mk(bdd_rt, &n_u2->rva,
                        _bdd_apply(bdd_rt,op,b1,u1,b2,n_u2->low,_errmsg),
                        _bdd_apply(bdd_rt,op,b1,u1,b2,n_u2->high,_errmsg),
                        _errmsg);
            } else { /* IS_LEAF(n_u2) || (cmp > 0) */
                u = bdd_mk(bdd_rt, &n_u1->rva,
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
    if ( !(bdd_rt = bdd_rt_init(&bdd_rt_struct,NULL,verbose/*verbose*/,_errmsg)) )
        return NULL;
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
    nodei ares = _bdd_apply(bdd_rt,op,b1,BDD_ROOT(b1),b2,BDD_ROOT(b2),_errmsg);
    if (ares == NODEI_NONE) {
        bdd_rt_free(bdd_rt);
        return NULL;
    }
    if ( V_rva_node_size(&bdd_rt->core.tree) == 2 ) {
        // there have no nodes been created, expression is constant, no errors
        V_rva_node_reset(&bdd_rt->core.tree);
        if ( ares == 0 ) {
            if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
        } else {
            if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
        }
    }
    bdd* res = serialize_bdd(&bdd_rt->core);
    bdd_rt_free(bdd_rt);
    //
    return res;
}

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
            
            if ( !IS_LEAF(node) ) { // INCOMPLETE not necessary starting at 2
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
    // fprintf(stdout,"_bdd_binary_op_by_text: %s\n",pbuff->buffer);
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

/*
 * BDD dot file generation
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
    bprintf(pbuff,"\tlabel=\"bdd(\'");
    bdd2string(pbuff,bdd,0);
    bprintf(pbuff,"\')\";\n");
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
 * BDD string conversion functions / bdd2string()
 */

#define BOOL_0(I)       ((I)==0)
#define BOOL_1(I)       ((I)==1)
#define BOOL_NODE(I)    ((I)<2)

/*
 * The D(T) define is used for debugging and documentation. In the expressen the
 * symbols have the following meaning:
 *  N           : current node 'v=d'
 *  L, R        : Low and High branch from this node.
 *  P(B)        : Genereted output of 'B' branch.
 *  '!' '&' '|' : boolean operators
 *  '0' '1'     : FALSE or TRUE constants
 */
// #define D(T)         fprintf(stdout,"+ [%s=%d]{L=%d,H=%d}\t--> %s\n",node->rva.var,node->rva.val,node->low,node->high,T)
#define D(T)

static void _bdd2string(pbuff *pb, bdd* bdd, nodei i) {
    rva_node *node = BDD_NODE(bdd,i);

    if ( IS_LEAF(node) ) {
        D("(0|1)");
        bprintf(pb,"%s",node->rva.var);
        return;
    } else {
        if ( BOOL_NODE(node->low) && BOOL_NODE(node->high) ) {
            if ( node->low == 1 ) { // negated
                D("! N");
                bprintf(pb,"!");
            } else 
                D("N");
            bprintf(pb,"%s=%d",node->rva.var,node->rva.val);
            return;
        } 
        bprintf(pb,"(");
        if ( BOOL_NODE(node->high) ) {
            if ( BOOL_1(node->high) ) {
                D("N | P(L)");
                bprintf(pb,"%s=%d|",node->rva.var,node->rva.val);
                _bdd2string(pb,bdd,node->low);
            } else { // BOOL_0(node->high)
                D("! N & P(L)");
                bprintf(pb,"!%s=%d&",node->rva.var,node->rva.val);
                _bdd2string(pb,bdd,node->low);
            }
        } else if ( BOOL_NODE(node->low) ) {
            if ( BOOL_0(node->low) ) {
                D("N & P(H)");
                bprintf(pb,"%s=%d&",node->rva.var,node->rva.val);
                _bdd2string(pb,bdd,node->high);
            } else { // BOOL_1(node->low
                D("! N | P(H)");
                bprintf(pb,"!%s=%d|",node->rva.var,node->rva.val);
                _bdd2string(pb,bdd,node->high);
            }
        } else { // Node without BOOL_NODE(Branch) children
            D("(N & P(H)) | P(L)");
            bprintf(pb,"%s=%d&",node->rva.var,node->rva.val);
            _bdd2string(pb,bdd,node->high);
            bprintf(pb,"|");
            _bdd2string(pb,bdd,node->low);
        }
        bprintf(pb,")");
    }
}

void bdd2string(pbuff* pb, bdd* bdd, int encapsulate) {
    if ( encapsulate ) bprintf(pb,"Bdd(");
    _bdd2string(pb,bdd,BDD_ROOT(bdd));
    if ( encapsulate ) bprintf(pb,")");
}

/*
 * BDD probability function
 */

static double bdd_probability_node(bdd_dictionary* dict, bdd* bdd, nodei i,char** extra,int verbose,char** _errmsg) {
    rva *rva = BDD_RVA(bdd,i);
    // INCOMPLETE: highly unefficient, store already computed results
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
        while ( IS_SAMEVAR(BDD_RVA(bdd,high),rva) ) {
            high = bdd_low(bdd,high);
        }
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
#ifdef BDD_VERBOSE
   if ( extra )
       sprintf(extra[i],"<i>(%.2f)<br/>%.2f</i>",p_root,res);
#endif
   return res;
}

double bdd_probability(bdd_dictionary* dict, bdd* bdd,char** extra, int verbose, char** _errmsg) {
    return bdd_probability_node(dict,bdd,BDD_ROOT(bdd),extra,verbose,_errmsg);
}

static int _contains_rva(bdd* bdd, char* var, int val) {
    for (int i=0; i<V_rva_node_size(&bdd->tree); i++) {
        rva_node* node = BDD_NODE(bdd,i);
        if ( !IS_LEAF(node)) {
            if ( COMPARE_VAR(var,node->rva.var) == 0) {
                 if ( (val < 0) || (val == node->rva.val) )
                     return 1;
            }
        }
    }
    return 0;
}

/* 
 * BDD property check function 
 */

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
