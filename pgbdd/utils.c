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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdnoreturn.h>

#include "utils.h"
#include "vector.h"

//
//
//

FILE* _debug_file = NULL;

void dp_print(const char *fmt,...)
{
    va_list ap;
    if ( _debug_file ) {
        va_start(ap, fmt);
        vfprintf(_debug_file, fmt, ap);
        va_end(ap);
        fflush(_debug_file);
    }
}

/*
 *
 *
 */

#define MAXERRMSG       256

int pg_error(char** _errmsg, const char *fmt,...)
{
    va_list ap;
    
    /*
     * Create error message buffer. In postgres context this is palloc'd
     * so it it pfree()'d after the aborted transaction.
     */
    *_errmsg = MALLOC(MAXERRMSG+2);
    va_start(ap, fmt);
    vsnprintf(*_errmsg, MAXERRMSG, fmt, ap);
    strcat(*_errmsg,"\n");
    va_end(ap);
    return 0;
}

int pg_fatal(const char *fmt,...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputs("\n",stderr);
    va_end(ap);
    exit(-1);
    return 0;
}

/*
 *
 *
 */

char* bdd_replace_str(char *dst, char* src, char *find, char replace) {
    int strlen_find    = strlen(find);

    int delta, dst_i = 0;
    char *cp, *last;
    last = cp = src;
    while ( (cp=strstr(cp,find)) ) {
        delta = (cp - last);
        memcpy(&dst[dst_i],last,delta);
        dst[dst_i+delta] = replace;
        dst_i += (delta+1);
        last = cp = cp + strlen_find;
    }
    delta = strlen(last);
    memcpy(&dst[dst_i],last,delta);
    dst[dst_i+delta] = 0;
    return dst;
} 

/*
 *
 *
 */

int bdd_atoi(char* a) {
    char* next;
    long val;
    errno = 0;
    val = strtol (a, &next, 10);
    if (errno == 0)
        return (int)val; // incomplete, can still be too big
    else 
        return -1;
}

double bdd_atof(char* a) {
    char * endptr;
    double d = strtod(a, &endptr);
    if (a == endptr)
        return (double)-1;
    else
        return d;
}

static u_int16_t const str100p[100] = {
  0x3030,0x3130,0x3230,0x3330,0x3430,0x3530,0x3630,0x3730,0x3830,0x3930,
  0x3031,0x3131,0x3231,0x3331,0x3431,0x3531,0x3631,0x3731,0x3831,0x3931,
  0x3032,0x3132,0x3232,0x3332,0x3432,0x3532,0x3632,0x3732,0x3832,0x3932,
  0x3033,0x3133,0x3233,0x3333,0x3433,0x3533,0x3633,0x3733,0x3833,0x3933,
  0x3034,0x3134,0x3234,0x3334,0x3434,0x3534,0x3634,0x3734,0x3834,0x3934,
  0x3035,0x3135,0x3235,0x3335,0x3435,0x3535,0x3635,0x3735,0x3835,0x3935,
  0x3036,0x3136,0x3236,0x3336,0x3436,0x3536,0x3636,0x3736,0x3836,0x3936,
  0x3037,0x3137,0x3237,0x3337,0x3437,0x3537,0x3637,0x3737,0x3837,0x3937,
  0x3038,0x3138,0x3238,0x3338,0x3438,0x3538,0x3638,0x3738,0x3838,0x3938,
  0x3039,0x3139,0x3239,0x3339,0x3439,0x3539,0x3639,0x3739,0x3839,0x3939,};

void fast_itoa(char *dst, u_int32_t val)
{
    char buf[16], *p = &buf[10];
    char* res;
    size_t res_len;

    *p = '\0';
    while(val >= 100)
    {
        u_int32_t const old = val;
        p -= 2;
        val /= 100;
        memcpy(p, &str100p[old - (val * 100)], sizeof(u_int16_t));
    }
    p -= 2;
    memcpy(p, &str100p[val], sizeof(u_int16_t));
    res =  &p[val < 10];
    res_len =  10 - (size_t)(res - buf);
    memcpy(dst,res,res_len+1);
}

/*
 *
 */

pbuff* pbuff_init(pbuff* pbuff) {
    pbuff->magic    = PBUFF_MAGIC;
    pbuff->size     = 0;
    pbuff->capacity = PBUFF_INITIAL_SIZE;
    pbuff->buffer   = pbuff->fast_buffer;
    pbuff->buffer[0]= 0;
    return pbuff;
}

pbuff* pbuff_reset(pbuff* pbuff) {
    PBUFF_ASSERT(pbuff);
    if ( pbuff->buffer != pbuff->fast_buffer )
        FREE(pbuff->buffer);
    return pbuff_init(pbuff);
}

char* pbuff_preserve_or_alloc(pbuff* pbuff) {
    PBUFF_ASSERT(pbuff);
    if ( pbuff->buffer != pbuff->fast_buffer )
        return pbuff->buffer; // preserve
    else {
        char* res;
        if ( !(res = (char*)MALLOC(pbuff->size+1)) )
                return 0;
        memcpy(res,pbuff->fast_buffer,pbuff->size+1);
        return res;
    }
}

void pbuff_free(pbuff* pbuff) {
    PBUFF_ASSERT(pbuff);
    pbuff->size = pbuff->capacity = -1;
    if ( pbuff->buffer != pbuff->fast_buffer )
        FREE(pbuff->buffer);
    pbuff->buffer = NULL;
}

void pbuff_flush(pbuff* pbuff, FILE* out) {
    PBUFF_ASSERT(pbuff);
    if ( out ) {
        fputs(pbuff->buffer, out);
    }
    pbuff->size      = 0;
    pbuff->buffer[0] = 0;
}


/* 
 * bprintf() is safe fprinf() like function which prints to a buffer. If 
 * printing fails 0 will be returned. The buffer is not infinite and after
 * PBUFF_MAX_TOTAL characters an overflow message will be printed at the end
 * of the buffer.
 */

static const char overflow_message[] = "[*PRINTBUFFER OVERFLOW*]\n";

int bprintf(pbuff* pbuff, const char *fmt,...)
{
    va_list ap;
    int  n_printed;
    char buffer[PBUFF_MAX_BPRINTF];
    PBUFF_ASSERT(pbuff);

    va_start(ap, fmt);
    n_printed = vsnprintf(buffer,PBUFF_MAX_BPRINTF,fmt,ap);
    va_end(ap);
    //
    if ( (pbuff->size+n_printed+1) > pbuff->capacity) {
        if ( pbuff->capacity > PBUFF_MAX_TOTAL) {
            // buffer is at its maximum size
            if ( !(pbuff->size > pbuff->capacity) ) { // alreay overflowing
                strcpy(&pbuff->buffer[pbuff->size -strlen(overflow_message)-1],
                       overflow_message);
                pbuff->size = pbuff->capacity + 1;
            }
            return 1;
        }
        pbuff->capacity *= 2;
        if ( pbuff->buffer == pbuff->fast_buffer )  {
            if ( !(pbuff->buffer = (char*)MALLOC(pbuff->capacity)) )
                return 0;
            memcpy(pbuff->buffer,pbuff->fast_buffer,pbuff->size+1);
        } else
            pbuff->buffer = (char*)REALLOC(pbuff->buffer,pbuff->capacity);
        if ( !pbuff->buffer )
            return 0;
    }
    memcpy(pbuff->buffer+pbuff->size,buffer,n_printed);
    pbuff->size += n_printed;
    pbuff->buffer[pbuff->size] = 0;
    return 1;
}

/*
 *
 * The bool_eval utility. The fastest possible boolean expression evaluator
 * for a boolean algebra with only ['0','1','!','&','|','(',')'] tokens!.
 * The implementation consists of a finite state machine with static table 
 * traversing part and a dynmic part which handles a stack of states used
 * for parenthese expression.
 *
 *
 */

// #define BEE_DEBUG

typedef unsigned char bee_state;

#define bee_s0              0
#define bee_s1              1
#define bee_snot            2
#define bee_s0and           3
#define bee_s0or            4
#define bee_s1and           5
#define bee_s1or            6
#define bee_salways0        7
#define bee_salways1        8
#define bee_svalstart       9
#define bee_NSTATES_STATIC 10
#define bee_sresult0       10
#define bee_sresult1       11
#define bee_sparopen       12
#define bee_error          13
#define bee_sparclose      14
#define bee_NSTATES        15

#define bee_0               0
#define bee_1               1
#define bee_not             2
#define bee_and             3
#define bee_or              4
#define bee_paropen         5
#define bee_parclose        6
#define bee_eof             7
#define bee_NTOKEN          8

static char *bee_token_STR[bee_NTOKEN] = {"0","1","!","&","|","(",")","E"};

char bee_token2char(bee_token bt) {
    if (bt >= bee_NTOKEN)
        return -1;
    else
        return *bee_token_STR[bt];
}

#ifdef BEE_DEBUG

static char *bee_state_STR[bee_NSTATES] = {
        "bee_s0", "bee_s1", "bee_snot", "bee_s0and", "bee_s0or", "bee_s1and",
        "bee_s1or", "bee_salways0", "bee_salways1", "bee_svalstart",
        "bee_sresult0", "bee_sresult1", "bee_sparopen", "bee_error", 
        "bee_sparclose" };


static char *bee_token_STR[bee_NTOKEN] = {"0","1","NOT","AND","OR","(",")","END"};

#endif

static bee_state fsm[bee_NSTATES_STATIC][bee_NTOKEN] = {
/*                  0   1   !   &   |   (   )   E  */
/*bee_s0*/       { 13, 13, 13,  3,  4, 13, 14, 10 },
/*bee_s1*/       { 13, 13, 13,  5,  6, 13, 14, 11 },
/*bee_snot*/     {  1,  0,  9, 13, 13, 12, 13, 13 },
/*bee_s0and*/    {  0,  0,  7, 13, 13, 12, 13, 13 },
/*bee_s0or*/     {  0,  1,  2, 13, 13, 12, 13, 13 },
/*bee_s1and*/    {  0,  1,  2, 13, 13, 12, 13, 13 },
/*bee_s1or*/     {  1,  1,  8, 13, 13, 12, 13, 13 },
/*bee_salways0*/ {  0,  0,  7, 13, 13, 12, 13, 13 },
/*bee_salways1*/ {  1,  1,  8, 13, 13, 12, 13, 13 },
/*bee_svalstart*/{  0,  1,  2, 13, 13, 12, 13, 13 }
};

static unsigned char bee_token_xlate[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,bee_not,255,
    255,255,255,bee_and,255,bee_paropen,bee_parclose,255,255,255,255,255,255,
    bee_0,bee_1,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,bee_or,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255};

int parse_tokens(char* p, char** _errmsg) {
    /* this functions assumes no spaces, they will generate an error */
    while (*p) {
        if ( (bee_token)(*p = bee_token_xlate[(int)*p]) == 255 ) {
            pg_error(_errmsg,"bee:parse_tokens: invalid char");
            return -1; 
        }
        p++;
    }
    *p = bee_eof;
    return 1;
}

typedef struct bee_state_stack {
    int        sp;
    bee_state  stack[BEE_STACKMAX];
} bee_state_stack;

static int bee_eval_fsm(char* t, char** _errmsg) {
    bee_state_stack ss = { .sp=0 };
    bee_state prev_state = bee_error;
    bee_state state      = bee_svalstart;

    while (1) {
#ifdef BEE_DEBUG
        fprintf(stderr,"- bee: start while = S[%s]\n",bee_state_STR[state]); 
#endif
        while ( state < bee_NSTATES_STATIC ) {
            bee_token new_token = (bee_token)*t++;
            prev_state = state;
            state      = fsm[state][new_token];
#ifdef BEE_DEBUG
            fprintf(stderr,"- bee_fsm: T[%s] >> S[%s]\n",bee_token_STR[new_token],bee_state_STR[state]); 
#endif
        }
#ifdef BEE_DEBUG
        fprintf(stderr,"- bee: start case = S[%s]\n",bee_state_STR[state]); 
#endif
        switch ( state ) {
         case bee_sparopen:
#ifdef BEE_DEBUG
            fprintf(stderr,"- push S[%s]\n",bee_state_STR[prev_state]); 
#endif
            ss.stack[ss.sp++] = prev_state;
            if ( ss.sp == BEE_STACKMAX ) {
                pg_error(_errmsg,"bee_eval: stack overflow");
                return -1;
            }
            state = bee_svalstart;
            break;
         case bee_sparclose: {
                bee_state pop_state;
                if ( (state < 2) || (ss.sp == 0) ) {
                    pg_error(_errmsg,"bee_eval: parenthese mismatch?");
                    return -1;
                }
                pop_state = ss.stack[--ss.sp];
#ifdef BEE_DEBUG
                fprintf(stderr,"- pop S[%s]\n",bee_state_STR[pop_state]); 
#endif
                state = fsm[pop_state][(bee_token)prev_state];
            }
            break;
         case bee_sresult0:
         case bee_sresult1:
            if (ss.sp != 0 ) {
                pg_error(_errmsg,"bee_eval: missing \')\'");
                return -1;
            }
#ifdef BEE_DEBUG
            fprintf(stderr,"- return S[%s]\n",bee_state_STR[state]); 
#endif
            return ( state - bee_sresult0); // 0 or 1
         case bee_error:
            pg_error(_errmsg,"bee_eval: syntax error");
            return -1;
         default:
            pg_error(_errmsg,"bee_eval: internal error");
            return -1;
        }
    }
    return -1; // should not happen
}

int bee_eval(char* boolean_expr, char** _errmsg) {
    return (parse_tokens(boolean_expr,_errmsg)!=-1) ? bee_eval_fsm(boolean_expr,_errmsg) : -1;
}

int bee_eval_raw(char* boolean_expr, char** _errmsg) {
    return bee_eval_fsm(boolean_expr,_errmsg);
}

