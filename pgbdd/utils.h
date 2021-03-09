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

#define START_DP(F) if( !_debug_file ) _debug_file=fopen(F,"ab")

void dp_print(const char *fmt,...) __attribute__ ((format (printf, 1, 2)));

#define DP if (_debug_file) dp_print

/*
 * usage:
 * START_DP("/tmp/PGLOG.TXT");
 * DP("+ dictionary_add() - 0\n"); // can be any printf style print
 */

/*
 * Buffer print functions, all printing should go to buffers
 */

#define PBUFF_MAX_BPRINTF       1024 /* max # printed chars in 1 bprintf() */
#define PBUFF_MAX_TOTAL   2147483647 /* max # printed chars in 1 buffer()  */
#define PBUFF_INITIAL_SIZE      1024 /* initial PBUFF size without alloc */

#define PBUFF_MAGIC          3789291 /* magic number should be OK */

#ifdef  BDD_OPTIMIZE
#define PBUFF_ASSERT(PB)     
#else
#define PBUFF_ASSERT(PB)     if (((PB)->magic!=PBUFF_MAGIC)||((PB)->size<0)||((PB)->size>(PB)->capacity)) pg_fatal("PBUFF_ASSERT FAILS")
#endif

typedef struct pbuff {
    int   size;
    int   capacity;
    int   magic;
    char* buffer;
    char  fast_buffer[PBUFF_INITIAL_SIZE];
} pbuff;

pbuff* pbuff_init(pbuff*);
pbuff* pbuff_reset(pbuff*f);
char*  pbuff_preserve_or_alloc(pbuff* pbuff);
void   pbuff_free(pbuff*);
void   pbuff_flush(pbuff*, FILE*);
int    bprintf(pbuff* pbuff, const char *fmt,...) __attribute__ ((format (printf, 2, 3)));

/*
 * Handling of errors and fatalities!
 *
 */

int pg_fatal(const char*,...) __attribute__ ((format (printf, 1, 2)));
int pg_error(char**, const char*,...) __attribute__ ((format (printf, 2, 3)));

/*
 * String and numeric utility functions
 */

int    bdd_atoi(char*);
double bdd_atof(char*);
void   fast_itoa(char *buf, u_int32_t val);

char* bdd_replace_str(char*,char*,char*,char);

/* 
 * The fast boolean expression evaluator (BEE)
 */

#define BEE_STACKMAX 1024 /* max stackdepth for boolean expressions */

typedef unsigned char bee_token;
char    bee_token2char(bee_token);

int parse_tokens(char*,char**);
int bee_eval_raw(char*,char**);
int bee_eval(char*,char**);

/*
 * The test function of this file!
 */

void test_utils(void);

#endif
