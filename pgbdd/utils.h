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
#ifndef UTILS_H
#define UTILS_H

//
//
//

extern FILE* _debug_file;

#define START_DP(F) _debug_file=fopen(F,"w")

void dp_print(const char *fmt,...);

#define DP if (_debug_file) dp_print

/*
 * usage:
 * START_DP("/tmp/PGLOG.TXT");
 * DP("+ dictionary_add() - 0\n"); // can be any printf style print
 */

//
//
//

int pg_error(char**, const char*,...);

bddstr create_bddstr(char*, int);
void bdd_print_V_bddstr(V_bddstr*, FILE*);

V_bddstr bdd_set_default_order(char*);

void bdd_print_row(bddrow, FILE*);

void bdd_print_tree(V_bddrow*, FILE*);
void bdd_generate_dot(bdd*, char* filename);

char* bdd_replace_str(char*,char*,char*,char*);

int    bdd_atoi(char*);
double bdd_atof(char*);

#define DEFAULT_PBUFF_SIZE      1024
// #define DEFAULT_PBUFF_SIZE      16

typedef struct pbuff {
    int   size;
    int   capacity;
    char* buffer;
    char  fast_buffer[DEFAULT_PBUFF_SIZE];
} pbuff;

pbuff* pbuff_init(pbuff*);
pbuff* pbuff_reset(pbuff*f);
char*  pbuff_preserve_or_alloc(pbuff* pbuff);
void   pbuff_free(pbuff*);
void   pbuff_flush(pbuff*, FILE*);
int    bprintf(pbuff* pbuff, const char *fmt,...);

int bdd_eval_bool(char*);

#endif
