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
#ifndef BDD_H
#define BDD_H

#define BDD_FAIL  0
#define BDD_OK    1

typedef char RVANAME[MAX_RVA_NAME];
#define      COMPARE_VAR(L,R) (strcmp(L,R))

// typedef int RVANAME;
// #define      COMPARE_VAR(L,R) ((L)-(R))

typedef struct rva {
    int         val;
    RVANAME     var;
} rva;

int cmpRva(rva*, rva*);

/*
 * The rva_node type defines a node in the bdd/graph tree. It has two
 * 'pointers' low and high pointing to the FALSE and TRUE children in the
 * tree. Best introduction to this graph is the "Introduction to Binary
 * Decision Diagrams" by HR Andersen.
 *
 * https://www.cmi.ac.in/~madhavan/courses/verification-2011/andersen-bdd.pdf
 *
 * The current implementation defines a node index type 'nodei' as a regular
 * short with the NODEI_NONE = -1. This can be made larger by making it
 * unsigned but BDD's of more than 32767 are too large to handle anyway I
 * think. And in this case NODEI_NONE would be USHRT_MAX and NODEI_MAX would
 * be USHRT_MAX-1
 */

typedef short      nodei;
#define NODEI_MAX  SHRT_MAX
#define NODEI_NONE -1

typedef struct rva_node {
    nodei low, high;
    rva   rva;
} rva_node;

DefVectorH(rva_node);

// The BDD core structure stored in the Postgres Database
typedef struct bdd {
    char     vl_len[4]; // used by Postgres memory management
    int      bytesize;  // size in bytes of serialized bdd
    V_rva_node tree;
    // because serialized tree grows in memory do not define attributes here!!!
} bdd;

/*
 *
 */

#ifdef BDD_VERBOSE
#define BDD_COUNT_RVA_INSTANTIATIONS
#endif

typedef unsigned short locptr;

#define LOC_EMPTY  USHRT_MAX

typedef struct rva_order {
    rva     rva; 
    locptr  loc;
#ifdef BDD_COUNT_RVA_INSTANTIATIONS
    short   bcount[2];
#endif
} rva_order;  

DefVectorH(rva_order);

int cmpRva_order(rva_order*,rva_order*);

typedef struct rva_epos { // position of rva in expression
    locptr  pos;  // position of rva in bbase text
    locptr  next; // next in chain of rva's, when empty -> LOC_EMPTY
} rva_epos;

/*
 *
 */

#define HASH_G(HM,L,R)  ((L*R)%hm->hash_sz)

#define HBI_MAX        USHRT_MAX
typedef short          hbi; // hash bucket index

typedef struct hb { // hash bucket def, size = 8
    nodei l;
    nodei r;
    nodei val;
    hbi   next; // next in bucket chain
} hb;

typedef struct hash_matrix {
    nodei sz_l;
    nodei sz_r;
    hbi   n_hb;
    hbi   max_hb;
    nodei hash_sz;
    hb*   v_hb;
    hbi   hash_tab[0];
} hash_matrix;

typedef struct bdd_runtime {
#ifdef BDD_VERBOSE
    char*     expr;             // base rva boolean expression
    int       verbose;
    int       mk_calls;
    int       check_calls;
    int       call_depth;
    int       G_cache_hits;
#endif
    hash_matrix* G_hash;
    //
    char*     e_stack;         // the boolean expr stack witch only '()!&|01'
    int       e_stack_len;     // length of the e_stack
    int       e_stack_framesz; // size of an expression stack block
    //
    int         n;             // number of distinct rva's in expression
    V_rva_order rva_order;     // main rva order array
    rva_epos*   rva_epos;      // positions of Rva in expression
    int         c_rva;     
    int         n_rva;         // total number of rva's in expression
    //
    bdd         core;
} bdd_runtime;

#define E_FRAME(BCTX,DEPTH) &((BCTX)->e_stack[(BCTX)->e_stack_framesz*(DEPTH)])

#ifdef BDD_OPTIMIZE
#define ORDER_SIZE(BCTX)  ((BCTX)->rva_order.size)
#define ORDER(BCTX,I)     (&(BCTX)->rva_order.items[I])
#else
#define ORDER_SIZE(BCTX)  V_rva_order_size(&(BCTX)->rva_order)
#define ORDER(BCTX,I)     V_rva_order_getp(&(BCTX)->rva_order,I)
#endif
#define ORDER_RVA(BCTX,I) &(ORDER(BCTX,I)->rva)

typedef struct bdd_alg bdd_alg; /*forward*/
typedef struct bdd_alg {
    char name[32];
    nodei  (*build)(bdd_alg*,bdd_runtime*,int,char**);
    nodei  (*mk)(bdd_runtime*,rva*,nodei,nodei,char**);
} bdd_alg;

extern bdd_alg *BDD_DEFAULT, *BDD_BASE;

bdd_alg* bdd_algorithm(char*, char** _errmsg);


#ifdef BDD_OPTIMIZE
#define BDD_NODE(PBDD,I)    (&(PBDD)->tree.items[I])
#define BDD_TREESIZE(PBDD)  ((PBDD)->tree.size)
#else
#define BDD_NODE(PBDD,I)    (V_rva_node_getp(&(PBDD)->tree,I))
#define BDD_TREESIZE(PBDD)  (V_rva_node_size(&(PBDD)->tree))
#endif

#define BDD_ROOT(PBDD)      (BDD_TREESIZE(PBDD)-1)

#define BDD_RVA(PBDD,I)     (&BDD_NODE(PBDD,I)->rva)
#define BDD_RVAVAR(PBDD,I)  (&(BDD_RVA(PBDD,I).var)

#define IS_LEAF(N)          (((N)->low==NODEI_NONE)&&((N)->high==NODEI_NONE))
#define IS_LEAF_I(PBDD,NI)  (IS_LEAF(BDD_NODE(PBDD,NI)))
#define LEAF_BOOLVALUE(N)   ((N)->rva.var[0]-'0')
#define NODE_BOOLVALUE(N)   (IS_LEAF(N) ? ((N)->rva.var[0]-'0') : -1)

#define bdd_low(PBDD,I)     (BDD_NODE(PBDD,I)->low)
#define bdd_high(PBDD,I)    (BDD_NODE(PBDD,I)->high)

#define IS_SAMEVAR(L,R)          (COMPARE_VAR((L)->var,(R)->var)==0)
#define IS_SAMEVAR_I(PBDD,LI,RI) (IS_SAMEVAR(&(BDD_RVA(PBDD,LI)),&(BDD_RVA(PBDD,RI))))

/*
 * Macros for Probdd
 */
#define LAST_ADDED(PBDD)          ((PBDD)->tree.size -1)
#define IS_LAST_ADDED(PBDD,I)     ((I)==LAST_ADDED(PBDD))
#define DELETE_LAST_ELEMENT(PBDD) V_rva_node_delete(&(PBDD)->tree,LAST_ADDED(PBDD))

//

bdd* create_bdd(bdd_alg*,char*,char**,int);

void bdd_rt_free(bdd_runtime*);

bdd* serialize_bdd(bdd*);
bdd* relocate_bdd(bdd*);

#define BDD_G_CACHE_MAX 65536

bdd*  bdd_apply(char,bdd*,bdd*,int,char**);

typedef enum op_mode {BY_TEXT, BY_APPLY} op_mode;

bdd*  bdd_operator(char,op_mode,bdd*,bdd*,char**);

int bdd_equal(bdd* lhs_bdd, bdd* rhs_bdd, char** _errmsg);
int bdd_equiv(bdd* lhs_bdd, bdd* rhs_bdd, char** _errmsg);

void  bdd2string(pbuff*,bdd*,int);
void  bdd_info(bdd*, pbuff*);
void  bdd_generate_dot(bdd*,pbuff*,char**);
void  bdd_generate_dotfile(bdd*,char*,char**);

double bdd_probability(bdd_dictionary*, bdd*,char**, int, char**);

#define BDD_IS_FALSE       0
#define BDD_IS_TRUE        1
#define BDD_HAS_VARIABLE   2
#define BDD_HAS_RVA        3

#define IS_VALID_PROPERTY(P)    (((P)>=0)&&((P)<4))

int    bdd_property_check(bdd*,int,char*,char**);
int    bdd_contains(bdd*,char*,int,char**);
bdd*   bdd_restrict(bdd*,char*,int,int,int,char**);

int    bdd_test_equivalence(char* l_expr, char* r_expr, char** _errmsg);

//
//
//

void test_bdd(void);

#endif
