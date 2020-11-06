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

typedef struct rva {
    char        var[MAX_RVA_NAME];
    int         val;
} rva;

DefVectorH(rva);

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

int cmpRva_node(rva_node*, rva_node*);

// #define BDD_STORE_EXPRESSION

// The BDD core structure stored in the Postgres Database
typedef struct bdd {
    char     vl_len[4]; // used by Postgres memory management
#ifdef BDD_STORE_EXPRESSION
    char    *expr; 
#endif
    int      bytesize;  // size in bytes of serialized bdd
    V_rva_node tree;
    // because serialized tree grows in memory do not define attributes here!!!
} bdd;

typedef struct bdd_runtime {
#ifdef BDD_VERBOSE
    int      verbose;
    int      mk_calls;
    int      check_calls;
    int      call_depth;
    int      G_cache_hits;
#endif
    char*    expr;
    int      len_expr;
    int      n;
    V_rva order;
    unsigned short    G_l;
    unsigned short    G_r;
    unsigned short*   G_cache;
    //
    bdd      core;
} bdd_runtime;

typedef struct bdd_alg bdd_alg; /*forward*/
typedef struct bdd_alg {
    char name[32];
    nodei  (*build)(bdd_alg*,bdd_runtime*,char*,int,char*,char**);
    nodei  (*mk)(bdd_runtime*,rva*,nodei,nodei,char**);
} bdd_alg;

extern bdd_alg *BDD_DEFAULT, *BDD_BASE, *BDD_KAJ, *BDD_ROBDD;

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

#define IS_LEAF(N)          (((N)->low==NODEI_NONE)&&((N)->high==NODEI_NONE))
#define IS_LEAF_I(PBDD,NI)  (IS_LEAF(BDD_NODE(PBDD,NI)))
#define LEAF_BOOLVALUE(N)   ((N)->rva.var[0]-'0')

#define IS_FALSE_I(PBDD,NI) (IS_LEAF(BDD_NODE(PBDD,NI)) && BDD_NODE(PBDD,NI)->rva.var[0]=='0')
#define IS_TRUE_I(PBDD,NI)  (IS_LEAF(BDD_NODE(PBDD,NI)) && BDD_NODE(PBDD,NI)->rva.var[0]=='1')

#define bdd_low(PBDD,I)     (BDD_NODE(PBDD,I)->low)
#define bdd_high(PBDD,I)    (BDD_NODE(PBDD,I)->high)

#define IS_SAMEVAR(L,R)          (strcmp((L)->var,(R)->var)==0)
#define IS_SAMEVAR_I(PBDD,LI,RI) (IS_SAMEVAR(&(BDD_NODE(PBDD,LI)->rva),&(BDD_NODE(PBDD,RI)->rva)))

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

#define BDD_G_CACHE_MAX 8192

bdd*  bdd_apply(char,bdd*,bdd*,int,char**);

typedef enum op_mode {BY_TEXT, BY_APPLY} op_mode;

bdd*  bdd_operator(char,op_mode,bdd*,bdd*,char**);

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

//
//
//

void test_bdd(void);

#endif
