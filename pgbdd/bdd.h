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

typedef struct bdd {
    char     vl_len[4]; // used by Postgres memory management
    int      verbose;
    char     name[MAXRVA];
    char    *expr; // incomplete, must be array in future
    int      expr_bufflen;      // 
    int      n;
    int      mk_calls;
    V_bddstr order;
    V_bddrow tree;
} bdd;

bddstr bdd_get_rva_name(bddstr);
int bdd_get_rva_value(bddstr);
int bdd_low(bdd*,int);
int bdd_high(bdd*,int);
int bdd_is_leaf(bdd*,int);

//

bdd* bdd_init(bdd*, char*, char*, V_bddstr*, int);
void bdd_free(bdd*);
void bdd_start_build(bdd*);

void test_bdd(void);

#endif
