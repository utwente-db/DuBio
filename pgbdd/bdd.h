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

#define BDD_NONE -1

typedef struct rva {
    char        var[MAX_RVA_NAME];
    int         val;
} rva;

DefVectorH(rva);

int cmpRva(rva*, rva*);

// 

typedef struct rva_node {
    rva rva;
    int low, high;
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
#endif
    char*    expr;
    int      len_expr;
    int      n;
    V_rva order;
    //
    bdd      core;
} bdd_runtime;

typedef struct bdd_alg bdd_alg; /*forward*/
typedef struct bdd_alg {
    char name[32];
    int  (*build)(bdd_alg*,bdd_runtime*,char*,int,char*,char**);
    int  (*mk)(bdd_alg*,bdd_runtime*,rva*,int,int,char**);
} bdd_alg;

extern bdd_alg *BDD_DEFAULT, *BDD_BASE, *BDD_KAJ, *BDD_ROBDD;

bdd_alg* bdd_algorithm(char*, char** _errmsg);


#ifdef BDD_OPTIMIZE
#define bdd_node(PBDD,I) (&(PBDD)->tree.items[I])
#else
#define bdd_node(PBDD,I) (V_rva_node_getp(&(PBDD)->tree,I))
#endif

#define bdd_rva(PBDD,I)  (&bdd_node(PBDD,I)->rva)

#define IS_LEAF(N)         (((N)->low==-1)&&((N)->high==-1))
#define IS_LEAF_I(PBDD,NI) (IS_LEAF(bdd_node(PBDD,NI)))

#define IS_FALSE_I(PBDD,NI) (IS_LEAF(bdd_node(PBDD,NI)) && bdd_node(PBDD,NI)->rva.var[0]=='0')
#define IS_TRUE_I(PBDD,NI) (IS_LEAF(bdd_node(PBDD,NI)) && bdd_node(PBDD,NI)->rva.var[0]=='1')

#define bdd_low(PBDD,I)  (bdd_node(PBDD,I)->low)
#define bdd_high(PBDD,I) (bdd_node(PBDD,I)->high)

#define IS_SAMEVAR(L,R)          (strcmp((L)->var,(R)->var)==0)
#define IS_SAMEVAR_I(PBDD,LI,RI) (IS_SAMEVAR(&(bdd_node(PBDD,LI)->rva),&(bdd_node(PBDD,RI)->rva)))

/*
 * Macros for Probdd
 */
#define LAST_ADDED(PBDD)          ((PBDD)->tree.size -1)
#define IS_LAST_ADDED(PBDD,I)     ((I)==LAST_ADDED(PBDD))
#define DELETE_LAST_ELEMENT(PBDD) V_rva_node_delete(&(PBDD)->tree,LAST_ADDED(PBDD))

//

bdd* create_bdd(bdd_alg*,char*,char**,int);
void bdd_free(bdd_runtime*);

bdd* serialize_bdd(bdd*);
bdd* relocate_bdd(bdd*);

bdd*  bdd_operator(char,bdd*,bdd*,char**);

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
