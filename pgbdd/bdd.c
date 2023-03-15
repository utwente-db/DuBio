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
 * + bdd2str may fail because of print buffer problems, should check
 */

/* There may be an 'error' during probability computation due to a very tiny 
 * rounding problem where a very small negative number is returned. We have
 * to look better into the problem but for now when this occurs we set the
 * return value to 0.0. 
 */
#define TINY_ROUNDING_FRACTION 1e-40

int cmpRva(rva* l, rva* r) {  
    int res = COMPARE_VAR(l->var,r->var);
    if ( res == 0 )
        res = (l->val - r->val);
    return res;
}

DefVectorC(rva_node);

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
    int est_sz = VECTOR_INIT_CAPACITY;
#ifdef BDD_VERBOSE
    bdd_rt->expr        = expr;
    if ( verbose ) 
        fprintf(stdout,"Create bdd: %s\n",(expr?expr:"NULL"));
    bdd_rt->verbose     = verbose;
    bdd_rt->mk_calls    = 0;
    bdd_rt->check_calls = 0;
#endif
    bdd_rt->n           = -1;
    bdd_rt->G_hash      = NULL;
    bdd_rt->rva_epos    = NULL;
    bdd_rt->e_stack     = NULL;
    // 
    if ( expr ) { // apply() does init without expr
        if ( !compute_rva_order(bdd_rt,expr,_errmsg) )
            return NULL;
        // INCOMPLETE, find better estimate for tree size
        est_sz = 2 * bdd_rt->n_rva;
        est_sz = (est_sz <= 256) ? est_sz : 256;
    }
    if ( !V_rva_node_init_estsz(&bdd_rt->core.tree,est_sz) ) {
        pg_error(_errmsg,"bdd_rt: error creating tree");
        return NULL;
    }
    return bdd_rt;
}

void bdd_rt_free(bdd_runtime* bdd_rt) {
    if ( bdd_rt->G_hash )
        FREE(bdd_rt->G_hash);
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
    if ( row->rva.val < 0 ) {
        bprintf(pbuff,"[\"%s\"]",row->rva.var);
    } else
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

static int bdd_frame_current_val(bdd_runtime* bctx, int depth, int order_nr) {
    if ( order_nr >= depth )
        return -1;
    else {
        char* frame = E_FRAME(bctx,depth);
        rva_order* rl = ORDER(bctx,order_nr);
        return (int)frame[bctx->rva_epos[rl->loc].pos];
    }
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

static void print_order_and_stack(bdd_runtime* bctx, pbuff* pbuff) {
    bprintf(pbuff,"# Rva order and frame stack\n");
    bprintf(pbuff,"---------------------------\n");
    bprintf(pbuff,"+ framesz = %d\n",bctx->e_stack_framesz);
    bprintf(pbuff,"+ stack_len = %d\n",bctx->e_stack_len);
    // bprintf(pbuff,"+ [n/c]_rva = %d/%d\n",bctx->n_rva,bctx->c_rva);
    bprintf(pbuff,"+ Sorted/Uniq RVA list (n=%d):\n",bctx->n);
    for (int i=0; i<ORDER_SIZE(bctx); i++) {
        rva_order* rl = ORDER(bctx,i);
        locptr p = rl->loc;
        
        bprintf(pbuff,"[%2d]: <%s=%d> pos{",i,rl->rva.var,rl->rva.val);
        while ( p != LOC_EMPTY ) {
            bprintf(pbuff,"%d",bctx->rva_epos[p].pos);
            p = bctx->rva_epos[p].next;
            if ( p != LOC_EMPTY )
                bprintf(pbuff,",");
        }
        bprintf(pbuff,"}");
#ifdef BDD_COUNT_RVA_INSTANTIATIONS
        bprintf(pbuff," \t#(0/1)=(%d/%d)",rl->bcount[0],rl->bcount[1]);
        bprintf(pbuff,"\n");
#endif
    }
    bprintf(pbuff,"+ baseframe = ");
    bdd_print_frame(bctx,0,1,pbuff);
    bprintf(pbuff,"\n");
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

static void bdd_print_rt(bdd_runtime* bdd_rt, pbuff* pbuff) {
    bprintf(pbuff,"+ expr     = %s\n",(bdd_rt->expr?bdd_rt->expr:NULL));
    bprintf(pbuff,"+ order    = ");
    bprintf(pbuff,"\n");
    bprintf(pbuff,"N          = %d\n",bdd_rt->n);
    bprintf(pbuff,"mk_calls   = %d\n",bdd_rt->mk_calls);
    bprintf(pbuff,"check_calls= %d\n",bdd_rt->check_calls);
    if (1) print_order_and_stack(bdd_rt,pbuff);
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
    // start iteration @max(low,high)+1 because it cannot be below this in tree
    for (nodei i=((low > high)?low:high)+1; i<tree->size; i++) {
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

static int count_rva(const char* p) {
    char c;
    int n_rva = 0;

    while ( (c=*p++) ) {
        if ( c == '=' ) 
            n_rva++;
    }
    return n_rva;
}

static int add2rva_order(bdd_runtime* bctx, char* var, int var_len, char* valp, char** _errmsg) {
    rva_order new_rva_order;
    int val;
    rva_order* rl = NULL;
    rva_order* rva_list;
    int l, r;
    rva_epos* rp;

    if ( var_len > MAX_RVA_NAME )
        return pg_error(_errmsg,"rva_name too long (max=%d) / %s",MAX_RVA_NAME, var);   
    memcpy(new_rva_order.rva.var,var,var_len);
    new_rva_order.rva.var[var_len] = 0;
    var = new_rva_order.rva.var;
    //
    val = bdd_atoi(valp);
    if ( val == NODEI_NONE )
        return pg_error(_errmsg,"bad rva value %s=%s",var,valp);   
    rva_list = bctx->rva_order.items;
    l = 0;
    r = bctx->rva_order.size-1;
    while (l<=r) 
    { 
        int m = l + (r-l)/2; 
        int cmp = strcmp(rva_list[m].rva.var,var);
        if ( cmp == 0 ) cmp = rva_list[m].rva.val - val;
        if (cmp == 0) { 
            rl = ORDER(bctx,m);;
            break;
        }
        if ( cmp<0 )  
            l = m + 1;  
        else 
            r = m - 1;  
    } 
    if ( !rl )  {
        int index;
        new_rva_order.rva.val = val;
        if ( (index = V_rva_order_insert_at(&bctx->rva_order,l,&new_rva_order)) < 0)
            return BDD_FAIL;
        rl      = ORDER(bctx,index);
        rl->loc = LOC_EMPTY;
    }
    rp = &(bctx->rva_epos[bctx->c_rva]);
    bctx->e_stack[bctx->e_stack_len] = '1'; // initial test value
    rp->pos  = bctx->e_stack_len++;
    rp->next = rl->loc;
    rl->loc = bctx->c_rva++;
    // 
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
                int len;

                while ( isalnum(*p) )
                    p++;
                len = p-start;;
                while ( *p && *p != '=' )
                    p++;
                if ( !(*p++ == '=') )
                    return pg_error(_errmsg,"missing \'=\' in expr: \"%s\"",start);
                while ( isspace(*p) ) 
                    p++;
                if ( !isdigit(*p) ) 
                    return pg_error(_errmsg,"missing value after \'=\' in expr: \"%s\"",start);
                if ( !add2rva_order(bctx,start,len,p,_errmsg) )
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
    bctx->n_rva   = count_rva(bdd_expr);
    V_rva_order_init_estsz(&bctx->rva_order,(bctx->n_rva<=128)?bctx->n_rva:128);
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
#ifdef BDD_COUNT_RVA_INSTANTIATIONS
    for (int i=0; i<bctx->n; i++) {
        rva_order* rl = ORDER(bctx,i);
        rl->bcount[0] = rl->bcount[1] = 0;
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
    rva_order* rl;

    memcpy(dstframe,srcframe,bctx->e_stack_framesz);
    rl = ORDER(bctx,depth);
    for(locptr p = rl->loc; p!=LOC_EMPTY; p = bctx->rva_epos[p].next)
        dstframe[bctx->rva_epos[p].pos] = v_0or1;
    return 1;
}

static nodei bctx_eval_top(bdd_runtime* bctx, int depth, char** _errmsg) {
    char* topframe = E_FRAME(bctx,depth);

    int new_res = bee_eval_raw(topframe, _errmsg);
    if ( new_res < 0 )
        return NODEI_NONE; 
#ifdef BDD_COUNT_RVA_INSTANTIATIONS
    for (int i=0; i<bctx->n; i++) {
        rva_order* rl = ORDER(bctx,i);
        short cval = bdd_frame_current_val(bctx,depth,i);
        rl->bcount[cval] += 1;
    }
#endif
    return (nodei)new_res;
}

/*
 * The BASE build()/mk() algorithms impplementing Kaj's algorithm
 */

static nodei bdd_mk(bdd_runtime* bdd_rt, rva *v, nodei l, nodei h, char** _errmsg) {
    nodei node, res;
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
    node = lookup_bdd_node(bdd_rt,v,l,h);
    if ( node != NODEI_NONE ) 
        return node; /* node already exists */ 
    res = bdd_create_node(&bdd_rt->core,v,l,h);
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
    rva* var;
    nodei l, h;
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
        int boolean_res;
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose ) {
            bprintf(pbuff,"EVAL{%s}[i=%d]: ",alg->name,depth);
            bdd_print_frame(bdd_rt,depth,0/*no rva*/,pbuff);
            bprintf(pbuff," = ");
            pbuff_flush(pbuff,stdout);
        }
#endif
        boolean_res = bctx_eval_top(bdd_rt,depth,_errmsg);
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose )
            bprintf(pbuff,"%d\n",boolean_res);
        pbuff_flush(pbuff,stdout);
        pbuff_free(pbuff);
#endif
        return (boolean_res < 0) ? NODEI_NONE : (nodei)boolean_res;
    }
    var = ORDER_RVA(bdd_rt,depth);
    bctx_orderi_set(bdd_rt,depth,0);
    // nodei l = alg->build(alg,bdd_rt,depth+1,_errmsg);
    l = bdd_build(alg,bdd_rt,depth+1,_errmsg);
    if ( l == NODEI_NONE ) return NODEI_NONE;
    bctx_orderi_set(bdd_rt,depth,1);
    _bctx_skip_samevar(bdd_rt,&depth);
    // nodei h = alg->build(alg,bdd_rt,depth,_errmsg);
    h = bdd_build(alg,bdd_rt,depth,_errmsg);
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
            int res;
            bctx_orderi_set(l_ctx,l_depth,0);
            res = _bdd_equiv(l_depth+1,l_ctx,r_depth,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
            bctx_orderi_set(l_ctx,l_depth,1);
            _bctx_skip_samevar(l_ctx,&l_depth);
            res = _bdd_equiv(l_depth,l_ctx,r_depth,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
        } else if (opt==2) {
            int res;
            bctx_orderi_set(r_ctx,r_depth,0);
            res = _bdd_equiv(l_depth,l_ctx,r_depth+1,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
            bctx_orderi_set(r_ctx,r_depth,1);
            _bctx_skip_samevar(r_ctx,&r_depth);
            res = _bdd_equiv(l_depth,l_ctx,r_depth,r_ctx,_errmsg);
            if ( res < 1 ) return res; // error or 'FALSE'
        } else { // opt == 3
            int res;
            bctx_orderi_set(l_ctx,l_depth,0);
            bctx_orderi_set(r_ctx,r_depth,0);
            res = _bdd_equiv(l_depth+1,l_ctx,r_depth+1,r_ctx,_errmsg);
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
            if ( 0 && !res ) {
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
    int res;

    if ( !bdd_rt_init(&l_ctx_s,l_expr,0,_errmsg) )
        return BDD_FAIL;
    if ( !bdd_rt_init(&r_ctx_s,r_expr,0,_errmsg) )
        return BDD_FAIL;
    res = _bdd_equiv(0, &l_ctx_s, 0, &r_ctx_s, _errmsg);
    //
    bdd_rt_free(&l_ctx_s);
    bdd_rt_free(&r_ctx_s);
    //
    return res;
}

static int bdd_start_build(bdd_alg* alg, bdd_runtime* bdd_rt, char** _errmsg) {
    nodei res;
#ifdef BDD_VERBOSE
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);

    if ( bdd_rt->verbose )
        fprintf(stdout,"BDD start_build\n");
    bdd_rt->mk_calls = 0;
    if ( bdd_rt->verbose ) {
        fprintf(stdout,"/-------START, alg=\"%s\"--------\n",alg->name);
        bdd_print_rt(bdd_rt,pbuff); pbuff_flush(pbuff,stdout);
        fprintf(stdout,"/--------------------------------\n");
    }
#endif
    if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE)==NODEI_NONE) return BDD_FAIL;
    if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE)==NODEI_NONE) return BDD_FAIL;
    res = alg->build(alg,bdd_rt,0,_errmsg);
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
        else {
            bdd_print_rt(bdd_rt,pbuff); pbuff_flush(pbuff,stdout);
        }
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
    bdd* res;

    if ( !(bdd_rt = bdd_rt_init(&bdd_struct,expr,verbose,_errmsg)) )
        return NULL;
    if ( ! bdd_start_build(alg,bdd_rt,_errmsg) ) {
        // something may be wrong because of error, do not free bdd_rt !!!
        return NULL;
    }
    res = serialize_bdd(&bdd_rt->core);
    bdd_rt_free(bdd_rt);

    return res;
}

#define BDD_BASE_SIZE   (sizeof(bdd) - sizeof(V_rva_node))

bdd* serialize_bdd(bdd* tbs) {
    int bytesize, tree_size;
    bdd* res = NULL;

    V_rva_node_shrink2size(&tbs->tree);
    tree_size= V_rva_node_bytesize(&tbs->tree);
    bytesize = BDD_BASE_SIZE + tree_size;
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

#define WORD_MODULO    sizeof(int)

#define HASH_MATRIX_SZ(SZ_H,SZ_B)  (sizeof(hash_matrix)+SZ_H*sizeof(hbi)+SZ_B*sizeof(hb))

static hash_matrix* create_G(nodei sz_l, nodei sz_r, char** _errmsg)  {
    nodei hash_sz = (sz_l+sz_r+WORD_MODULO - 1)/WORD_MODULO*WORD_MODULO;
    hash_matrix* res;

    if ( !(res = MALLOC(HASH_MATRIX_SZ(hash_sz,hash_sz))) ) {
        pg_error(_errmsg,"create_G: alloc fails");
        return NULL;
    }
    res->sz_l    = sz_l;
    res->sz_r    = sz_r;
    res->hash_sz = hash_sz;
    res->max_hb  = hash_sz;
    res->n_hb    = 0;
    res->v_hb    = (hb*)&res->hash_tab[res->hash_sz]; // area after hash area
    for(int i=0; i<res->hash_sz; i++)
        res->hash_tab[i] = -1;
    return res;
}

/* 
 * static void print_G(hash_matrix *hm)  {
 *     fprintf(stdout,"+ sz_l     = %d\n",(int)hm->sz_l);
 *     fprintf(stdout,"+ sz_r     = %d\n",(int)hm->sz_r);
 *     fprintf(stdout,"+ hash_sz  = %d\n",(int)hm->hash_sz);
 *     fprintf(stdout,"+ n_hb     = %d\n",(int)hm->n_hb);
 *     fprintf(stdout,"+ max_hb   = %d\n",(int)hm->max_hb);
 *     fprintf(stdout,"+ hash_tab = [");
 *         for(int i=0; i<hm->hash_sz; i++) {
 *             hbi bptr = hm->hash_tab[i];
 *             int cnt = 0;
 *  
 *             while ( bptr >= 0 ) {
 *                 hb* b = &hm->v_hb[bptr];
 *                 bptr = b->next;
 *                 cnt++;
 *             }
 *             fprintf(stdout,"%d[#%d] ",(int)hm->hash_tab[i],cnt);
 *         }
 *     fprintf(stdout,"]\n");
 * }
 */

static hash_matrix* extend_G(hash_matrix *hm, char** _errmsg)  {
    hash_matrix* res;
    if ( !(res = (hash_matrix*)REALLOC(hm, HASH_MATRIX_SZ(hm->hash_sz,2 * hm->max_hb))) ) {
        pg_error(_errmsg,"extend_G: alloc fails");
        return NULL;
    }
    res->max_hb  = 2 * res->max_hb;
    res->v_hb    = (hb*)&res->hash_tab[res->hash_sz]; // area after hash area
    return res;
}

static nodei lookup_G(hash_matrix *hm, nodei l, nodei r)  {
    hbi bptr = hm->hash_tab[HASH_G(hm,l,r)];

#ifdef BDD_VERBOSE
    if ( l<0 || r<0 || l>=hm->sz_l || r>=hm->sz_r) 
        pg_fatal("*_G: l(%d),r(%d) index out of range",(int)l,(int)r);
#endif
    while ( bptr >= 0 ) {
        hb* b = &hm->v_hb[bptr];
        if ( (b->l==l) && (b->r==r) )
            return b->val;
        bptr = b->next;
    }
    return NODEI_NONE;
}

static hash_matrix* store_G(hash_matrix *hm, nodei l, nodei r, nodei val, char** _errmsg)  {
    hbi bptr = HASH_G(hm,l,r);
    hb* b;
#ifdef BDD_VERBOSE
    if ( l<0 || r<0 || l>=hm->sz_l || r>=hm->sz_r) 
        pg_fatal("*_G: l(%d),r(%d) index out of range",(int)l,(int)r);
#endif
    if (hm->n_hb >= hm->max_hb) {
        if ( !(hm = extend_G(hm,_errmsg)) )
            return NULL;
    }
    b = &hm->v_hb[hm->n_hb];
    b->l = l;
    b->r = r;
    b->val = val;
    b->next = hm->hash_tab[bptr];
    hm->hash_tab[bptr] = hm->n_hb++;
    return hm;
}

/*
 * static void test_hashtable()  {
 *     int i, j;
 *     hash_matrix* hm;
 * 
 *     hm = create_G(10,20);
 *     for(i=0; i<10; i++) for(j=0; j<20; j++) {
 *         hm = H_store_G(hm,i,j,i*j);
 *     }
 *     // print_G(hm);
 *     for(i=0; i<10; i++) for(j=0; j<20; j++) {
 *         if ( H_lookup_G(hm,i,j) != (i*j) )
 *             pg_fatal("UNEXPECTED");
 *     }
 *     FREE(hm);
 * }
 */

static nodei _bdd_apply(bdd_runtime* bdd_rt, char op, bdd* b1, nodei u1, bdd* b2, nodei u2, char** _errmsg)
{
    nodei u;

#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        bdd_rt->call_depth++;
        // for(int i=0;i<bdd_rt->call_depth; i++)
        //     bprintf(pbuff,">>");
        bprintf(pbuff," _bdd_apply(%c,",op);
        bdd_print_row(b1,u1,pbuff);
        bprintf(pbuff," , ");
        bdd_print_row(b2,u2,pbuff);
        bprintf(pbuff,")\n");
        pbuff_flush(pbuff,stdout);
    }
#endif
    if ( (u = lookup_G(bdd_rt->G_hash,u1,u2)) < 0 ) {
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
#ifdef BDD_VERBOSE
        if ( bdd_rt->verbose ) {
            fprintf(stdout,"+ store_G(%d,%d) = %d\n",(int)u1,(int)u2,(int) u);
        }
#endif
        if ( !(bdd_rt->G_hash = store_G(bdd_rt->G_hash,u1,u2,u,_errmsg)) )
            return NODEI_NONE;
    }
#ifdef BDD_VERBOSE
        else if ( bdd_rt->verbose ) {
            fprintf(stdout,"+ lookup_G(%d,%d) = %d\n",(int)u1,(int)u2,(int) u);
        }
#endif
#ifdef BDD_VERBOSE
    bdd_rt->call_depth--;
#endif
    return u;
}

bdd* bdd_apply(char op,bdd* b1,bdd* b2,int verbose, char** _errmsg) {
    bdd_runtime bdd_rt_struct, *bdd_rt;
    nodei ares;
    bdd*  res;

    if ( !(bdd_rt = bdd_rt_init(&bdd_rt_struct,NULL,verbose/*verbose*/,_errmsg)) )
        return NULL;
#ifdef BDD_VERBOSE
    if ( bdd_rt->verbose ) {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        bdd_rt->call_depth++;
        // for(int i=0;i<bdd_rt->call_depth; i++)
        //     bprintf(pbuff,">>");
        bprintf(pbuff,"+ TOP LEVEL: bdd_apply(%c,",op);
        bdd2string(pbuff,b1,0);
        bprintf(pbuff," , ");
        bdd2string(pbuff,b2,0);
        bprintf(pbuff,")\n");
        pbuff_flush(pbuff,stdout);
        pbuff_free(pbuff);
    }
#endif
    if ( !(bdd_rt->G_hash = create_G(BDD_ROOT(b1)+1,BDD_ROOT(b2)+1,_errmsg)) )
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
    ares = _bdd_apply(bdd_rt,op,b1,BDD_ROOT(b1),b2,BDD_ROOT(b2),_errmsg);
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
    res = serialize_bdd(&bdd_rt->core);
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
    bdd* res;

    bprintf(pbuff,"(");
    bdd2string(pbuff,lhs_bdd,0);
    bprintf(pbuff,")%c(",operator);
    bdd2string(pbuff,rhs_bdd,0);
    bprintf(pbuff,")");
    // fprintf(stdout,"_bdd_binary_op_by_text: %s\n",pbuff->buffer);
    res = create_bdd(BDD_DEFAULT,pbuff->buffer,_errmsg,0/*verbose*/);
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
        if ( m == BY_APPLY )
            return bdd_apply(operator,lhs,rhs,0,_errmsg);
        else
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
    FILE* f;
    bdd_generate_dot(bdd,pbuff,extra);
    f = fopen(dotfile,"w");
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
 *  P(B)        : Generated output of 'B' branch.
 *  '!' '&' '|' : boolean operators
 *  '0' '1'     : FALSE or TRUE constants
 */
// #define D(T)         fprintf(stdout,"+ {%2d}[%s=%2d]{L=%2d,H=%2d} --> %14s\t%s\n",i,node->rva.var,node->rva.val,node->low,node->high,T,pb->buffer)
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
                D("N | P(L)");
                bprintf(pb,"!(%s=%d)&",node->rva.var,node->rva.val);
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
            D("(N & P(H)) | (!N & P(L))");
            bprintf(pb,"(%s=%d&",node->rva.var,node->rva.val);
            _bdd2string(pb,bdd,node->high);
            bprintf(pb,")|(!%s=%d&",node->rva.var,node->rva.val);
            _bdd2string(pb,bdd,node->low);
            bprintf(pb,")");
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

static double bdd_probability_node(bdd_dictionary* dict, bdd* bdd, nodei T, char** extra,int verbose,char** _errmsg) {
    nodei TT;
    rva_node *n_T, *n_TT;
    double m;
    double p, P_n;

    n_T = n_TT = BDD_NODE(bdd,T);
    if ( IS_LEAF(n_T) ) {
        p = P_n = LEAF_BOOLVALUE(n_T) ? 1.0 : 0.0;
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout,"+NODE[#%d]:LEAF=%f)\n",T,p);
#endif
    } else {
        double P_check = -1.0;

        P_n = lookup_probability(dict,&n_T->rva);
        if ( P_n < 0.0 ) {
            char *str_rep;

            pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
            bdd2string(pbuff,bdd,0);
            str_rep = MALLOC(pbuff->size+1);
            memcpy(str_rep, pbuff->buffer, pbuff->size+1);
            pbuff_free(pbuff);
            pg_error(_errmsg,"dictionary_lookup: rva[\'%s\'] not found in %s.",n_T->rva.var, str_rep);
            return -1.0;
        }
        m = 1.0 - P_n;
        P_check = bdd_probability_node(dict,bdd,n_TT->high,extra,verbose,_errmsg) * P_n;
        if ( P_check < 0 )
            return P_check;
        p = P_check;
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout,"+NODE[#%d]:START: %s=%d, m=%f, p=%f\n",T,n_T->rva.var,n_T->rva.val,m,p);
#endif
        if ( IS_SAMEVAR(BDD_RVA(bdd,n_TT->high),&n_T->rva) ) {
            pg_error(_errmsg,"probabilty_alg:loop: unexpected var \'%s\' high branch",n_T->rva.var);
            return -1.0;
        }
        while ( IS_SAMEVAR(BDD_RVA(bdd,n_TT->low),&n_T->rva) ) {
            TT = n_TT->low;
            n_TT = BDD_NODE(bdd,TT);
            if ( IS_SAMEVAR(BDD_RVA(bdd,n_TT->high),&n_T->rva) ) {
                pg_error(_errmsg,"probabilty_alg: unexpected var \'%s\' high branch",n_T->rva.var);
                return -1.0;
            }
            P_n = lookup_probability(dict,&n_TT->rva);
            if ( P_n < 0.0 ) {
                pg_error(_errmsg,"dictionary_lookup: rva[\'%s\'] not found.",n_TT->rva.var);
                return -1.0;
            }
            m = m - P_n;
            P_check = bdd_probability_node(dict,bdd,n_TT->high,extra,verbose,_errmsg);
            if ( P_check < 0 )
                return P_check;
            p =  p + P_check * P_n;
#ifdef BDD_VERBOSE
            if ( verbose )
                fprintf(stdout,"+NODE[#%d]:SAMEVAR-LOOP: %s=%d, P_n=%f, m=%f, p=%f\n",T,n_TT->rva.var, n_TT->rva.val, P_n, m, p);
#endif
        }
        p =  p + bdd_probability_node(dict,bdd,n_TT->low,extra,verbose,_errmsg) * m;
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout,"+NODE[#%d]:END: p=%f\n",T, p);
#endif
   }
   if ( extra )
       sprintf(extra[T],"<i>(%.3f)<br/>%.3f<br/>%d</i>",lookup_probability(dict,BDD_RVA(bdd,T)),p,T);
#ifdef BDD_VERBOSE
        if ( verbose )
            fprintf(stdout, "+**NODE[#%d]:result=%f\n",T,p);
#endif
   /* INCOMPLETE: check if prob is a very small negative number and if soo make it 0
    */
   if ( p < 0.0 || p > 1.0 ) {
       char *str_rep;

       pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
       bdd2string(pbuff,bdd,0);
       str_rep = MALLOC(pbuff->size+1);
       memcpy(str_rep, pbuff->buffer, pbuff->size+1);
       pbuff_free(pbuff);
       pg_error(_errmsg,"probability_check: probvalue %f out of range: %s", p, str_rep);
       return -1.0;
   }
   if ((p < 0.0) && (p > -TINY_ROUNDING_FRACTION))
      p = 0.0;
   return p;
}

double bdd_probability(bdd_dictionary* dict, bdd* bdd,char** extra, int verbose, char** _errmsg) {
    return bdd_probability_node(dict,bdd,BDD_ROOT(bdd),extra,verbose,_errmsg);
}

/* 
 * BDD contains function to check if a bdd contains an rva
 */

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

int bdd_contains(bdd* bdd, char* var, int val, char** _errmsg) {
    return _contains_rva(bdd,var,val);
}

/* 
 * BDD property check function 
 */

// INCOMPLETE, no parsing in this fun, var and val must be seperate args
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
                    int val = bdd_atoi(++valp);
                    char toggle = *eov;
                    int res = -1;
                    *eov = 0;
                    if ( val != NODEI_NONE) {
                        res = _contains_rva(bdd,varp,val);
                    }
                    *eov = toggle; // restore string
                    return res;
                }
            }
        }
        pg_error(_errmsg,"bdd_property_check: bad rva (%s)",s);
        return -1;
    }
    return 0;
}

/* 
 * BDD restrict function 
 */

static nodei _bdd_restrict(bdd_runtime* bdd_rt, bdd* p_bdd, nodei p_u, char* var, int val, int torf, char** _errmsg)
{
    nodei r_u = NODEI_NONE;

    rva_node *n_u = BDD_NODE(p_bdd,p_u);
    if ( IS_LEAF(n_u) )
        r_u = p_u;
    else
        if ( (COMPARE_VAR(var,n_u->rva.var) == 0) && ((val < 0) || (val == n_u->rva.val)) )
            r_u =_bdd_restrict(bdd_rt,p_bdd,(torf?n_u->high:n_u->low), var,val,torf,_errmsg);
        else
            r_u = bdd_mk(bdd_rt, &n_u->rva,
                   _bdd_restrict(bdd_rt,p_bdd,n_u->low, var,val,torf,_errmsg),
                   _bdd_restrict(bdd_rt,p_bdd,n_u->high,var,val,torf,_errmsg),
                   _errmsg);
    return r_u;
}

bdd* bdd_restrict(bdd* p_bdd, char* var, int val, int torf, int verbose, char** _errmsg) {
    bdd_runtime bdd_rt_struct, *bdd_rt;
    nodei rres;
    bdd*  res;

    if ( !(bdd_rt = bdd_rt_init(&bdd_rt_struct,NULL,verbose/*verbose*/,_errmsg)) )
        return NULL;
    if ( (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE)==NODEI_NONE) ||
         (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE)==NODEI_NONE) ) {
        pg_error(_errmsg,"bdd_restrict: tree init [0,1] fails");
        return NULL;
    }
    //
    rres = _bdd_restrict(bdd_rt,p_bdd,BDD_ROOT(p_bdd),var,val,torf,_errmsg);
    if (rres == NODEI_NONE) {
        bdd_rt_free(bdd_rt);
        return NULL;
    }
    //
    if ( V_rva_node_size(&bdd_rt->core.tree) == 2 ) {
        // there have no nodes been created, expression is constant, no errors
        V_rva_node_reset(&bdd_rt->core.tree);
        if ( rres == 0 ) {
            if (bdd_create_node(&bdd_rt->core,&RVA_0,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
        } else {
            if (bdd_create_node(&bdd_rt->core,&RVA_1,NODEI_NONE,NODEI_NONE) == NODEI_NONE) return BDD_FAIL;
        }
    }
    res = serialize_bdd(&bdd_rt->core);
    bdd_rt_free(bdd_rt);
    //
    return res;
}

/* 
 * BDD equal and equivalent function 
 */

int bdd_equal(bdd* lhs_bdd, bdd* rhs_bdd, char** _errmsg) {
    if ( BDD_TREESIZE(lhs_bdd) != BDD_TREESIZE(rhs_bdd) )
        return 0; 
    for(nodei i=0; i<BDD_TREESIZE(lhs_bdd); i++) {
        rva_node* l = BDD_NODE(lhs_bdd,i);
        rva_node* r = BDD_NODE(rhs_bdd,i);
        if ( (l->low!=r->low) || (l->high!=r->high) || (cmpRva(&l->rva,&r->rva)!=0) )
            return 0;
    }
    return 1;
}

int bdd_equiv(bdd* lhs_bdd, bdd* rhs_bdd, char** _errmsg) {
    int res = -1;
    pbuff l_pbuff_struct, *l_pbuff=pbuff_init(&l_pbuff_struct);
    pbuff r_pbuff_struct, *r_pbuff=pbuff_init(&r_pbuff_struct);
    bdd2string(l_pbuff,lhs_bdd,0);
    bdd2string(r_pbuff,rhs_bdd,0);
    res = bdd_test_equivalence(l_pbuff->buffer,r_pbuff->buffer,_errmsg);
    pbuff_free(l_pbuff);
    return res;
}

