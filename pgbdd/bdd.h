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

#define MAXRVA   32
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
    V_bddstr order;
    //
    bdd      core;
} bdd_runtime;

bddstr bdd_get_rva_name(bddstr);
int bdd_get_rva_value(bddstr);
int bdd_low(bdd_runtime*,int);
int bdd_high(bdd_runtime*,int);
int bdd_is_leaf(bdd_runtime*,int);

//

bdd_runtime* bdd_init(bdd_runtime*, char*, V_bddstr*, int);
void bdd_free(bdd_runtime*);
void bdd_start_build(bdd_runtime*);

bdd* create_bdd(char* expr, char **_errmsg, int verbose);
bdd* relocate_bdd(bdd*);

void test_bdd(void);

#endif
