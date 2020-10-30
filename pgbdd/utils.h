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

/*
 * Debugging functions for use 'in' the posgres engine
 */

extern FILE* _debug_file;

#define START_DP(F) _debug_file=fopen(F,"w")

void dp_print(const char *fmt,...);

#define DP if (_debug_file) dp_print

/*
 * usage:
 * START_DP("/tmp/PGLOG.TXT");
 * DP("+ dictionary_add() - 0\n"); // can be any printf style print
 */

/*
 * Buffer print functions, all printing should go to buffers
 */

#define MAX_BPRINTF             1024 /* max # printed chars in 1 bprintf() */

#define DEFAULT_PBUFF_SIZE      1024 /* initial PBUFF size without alloc */

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

/*
 * Handling of errors and fatalities!
 *
 */

int pg_fatal(const char*,...);
int pg_error(char**, const char*,...);

/*
 * String and numeric utility functions
 */

int    bdd_atoi(char*);
double bdd_atof(char*);
void   fast_itoa(char *buf, uint32_t val);

char* bdd_replace_str(char*,char*,char*,char);

/* 
 * The fast boolean expression evaluator (BEE)
 */

#define BEE_STACKMAX 1024 /* max stackdepth for boolean expressions */

int bee_eval(char*,char**);

/*
 * The test function of this file!
 */

void test_utils(void);

#endif
