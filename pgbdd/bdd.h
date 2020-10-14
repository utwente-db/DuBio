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

int cmpBddstr(bddstr, bddstr);

// 

typedef struct bddrow {
    char rva[MAXRVA];
    int  low, high;
} bddrow;

DefVectorH(bddrow);

int cmpBddrow(bddrow, bddrow);

// 

typedef struct bdd {
    int      verbose;
    char     name[MAXRVA];
    char    *expr; // incomplete, must be array in future
    int      expr_bufflen;
    V_bddrow tree;
    int      n;
    int      mk_calls;
    V_bddstr order;
} bdd;

//

void test_bdd();

#endif
