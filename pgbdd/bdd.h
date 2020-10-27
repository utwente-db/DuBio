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
    uint32_t    val;
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

// The BDD core structure stored in the Postgres Database
typedef struct bdd {
    char     vl_len[4]; // used by Postgres memory management
    char    *expr; 
    int      bytesize;  // size in bytes of serialized bdd
    V_rva_node tree;
    // because serialized tree grows in memory do not define attributes here!!!
} bdd;

typedef struct bdd_runtime {
    int      verbose;
    int      expr_bufflen;
    int      n;
    int      mk_calls;
    int      check_calls;
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

extern bdd_alg *BDD_BASE, *BDD_KAJ, *BDD_PROBBDD;

int   bdd_low(bdd*,int);
int   bdd_high(bdd*,int);
int   bdd_is_leaf(bdd*,int);

rva*  bdd_rva(bdd*,int);
int   rva_is_samevar(rva*, rva*);

//

void bdd_free(bdd_runtime*);
bdd* serialize_bdd(bdd*);

bdd* create_bdd(bdd_alg*,char*,char**,int);
void bdd_info(bdd*, pbuff*);
bdd* relocate_bdd(bdd*);

int create_rva(rva*,char*,int,char**);
void bdd_print_V_rva(V_rva*, pbuff*);

int bdd_set_default_order(V_rva*,char*,char**);

void bdd_print_tree(V_rva_node*, pbuff*);

void bdd_generate_dot(bdd*,pbuff*,char**);
void bdd_generate_dotfile(bdd*,char*,char**);

double bdd_probability(bdd_dictionary*, bdd*,char**, int, char**);

//
//
//

void test_bdd(void);

#endif
