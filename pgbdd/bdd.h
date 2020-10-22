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

typedef struct bddstr {
    char str[MAXRVA];
} bddstr;

DefVectorH(bddstr);

int cmpBddstr(bddstr*, bddstr*);

// 

typedef struct bddrow {
    char rva[MAXRVA];
    int  low, high;
} bddrow;

DefVectorH(bddrow);

int cmpBddrow(bddrow*, bddrow*);

// 

// The BDD core structure stored in the Postgres Database
typedef struct bdd {
    char     vl_len[4]; // used by Postgres memory management
    char    *expr; 
    int      bytesize;  // size in bytes of serialized bdd
    V_bddrow tree;
    // because serialized tree grows in memory do not define attributes here!!!
} bdd;

typedef struct bdd_runtime {
    int      verbose;
    int      expr_bufflen;
    int      n;
    int      mk_calls;
    int      check_calls;
    V_bddstr order;
    //
    bdd      core;
} bdd_runtime;

bddstr bdd_get_rva_name(bddstr);
char* bdd_rva(bdd*,int);
int   bdd_get_rva_value(bddstr);
int   bdd_low(bdd*,int);
int   bdd_high(bdd*,int);
int   bdd_is_leaf(bdd*,int);

//

bdd_runtime* bdd_init(bdd_runtime*, char*, V_bddstr*, int);
void bdd_free(bdd_runtime*);
bdd* serialize_bdd(bdd*);

bdd* create_bdd(char* expr, char **_errmsg, int verbose);
void bdd_info(bdd*, pbuff*);
bdd* relocate_bdd(bdd*);

bddstr create_bddstr(char*, int);
void bdd_print_V_bddstr(V_bddstr*, pbuff*);

V_bddstr bdd_set_default_order(char*);

void bdd_print_tree(V_bddrow*, pbuff*);

void bdd_generate_dot(bdd*,pbuff*,char**);
void bdd_generate_dotfile(bdd*,char*,char**);

double bdd_probability(bdd_dictionary*, bdd*,char**, int, char**);

void test_bdd(void);

#endif
