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

#include "vector.h"
#include "utils.h"

//
//
//

int   _debug_flag = 0;
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

int pg_error(char** errmsg, const char *fmt,...)
{
    va_list ap;
    
    /*
     * Create error message buffer. In postgres context this is palloc'd
     * so it it pfree()'d after the aborted transaction.
     */
    *errmsg = MALLOC(MAXERRMSG+2);
    va_start(ap, fmt);
    vsnprintf(*errmsg, MAXERRMSG, fmt, ap);
    strcat(*errmsg,"\n");
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
/*
 *
 *
 */

char* bdd_replace_str(char *dst, char* src, char *find, char *replace) {
    int strlen_find    = strlen(find);
    int strlen_replace = strlen(replace);

    if (strlen_replace > strlen_find)
        vector_error("bdd_replace_str: replace > find");
    int delta, dst_i = 0;
    char *cp, *last;
    last = cp = src;
    while ( (cp=strstr(cp,find)) ) {
        delta = (cp - last);
        memcpy(&dst[dst_i],last,delta);
        memcpy(&dst[dst_i+delta],replace,strlen_replace);
        dst_i += (delta+strlen_replace);
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
    errno = 0;
    char* next;
    long val = strtol (a, &next, 10);
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

pbuff* pbuff_init(pbuff* pbuff) {
    pbuff->size     = 0;
    pbuff->capacity = DEFAULT_PBUFF_SIZE;
    pbuff->buffer   = pbuff->fast_buffer;
    pbuff->buffer[0]= 0;
    return pbuff;
}

pbuff* pbuff_reset(pbuff* pbuff) {
    if ( pbuff->buffer != pbuff->fast_buffer )
        FREE(pbuff->buffer);
    return pbuff_init(pbuff);
}

char* pbuff_preserve_or_alloc(pbuff* pbuff) {
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
    pbuff->size = pbuff->capacity = -1;
    if ( pbuff->buffer != pbuff->fast_buffer )
        FREE(pbuff->buffer);
    pbuff->buffer = NULL;
}

void pbuff_flush(pbuff* pbuff, FILE* out) {
    fputs(pbuff->buffer, out);
    pbuff->size      = 0;
    pbuff->buffer[0] = 0;
}

int bprintf(pbuff* pbuff, const char *fmt,...)
{
    va_list ap;
    char buffer[1024];

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);
    //
    int size = strlen(buffer);
    if ( (pbuff->size+size+1) > pbuff->capacity) {
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
    memcpy(pbuff->buffer+pbuff->size,buffer,size);
    pbuff->size += size;
    pbuff->buffer[pbuff->size] = 0;
    return 1;
}

/*
 *
 *
 */

static int op_preced(const char c)
{
    switch(c)    {
        case '|':
            return 6;
        case '&':
            return 5;
        case '!':
            return 4;
        case '*':  case '/': case '%':
            return 3;
        case '+': case '-':
            return 2;
        case '=':
            return 1;
    }
    return 0;
}

static int op_left_assoc(const char c)
{
    switch(c)    {
        // left to right
        case '*': case '/': case '%': case '+': case '-': case '|': case '&':
            return 1;
        // right to left
        case '=': case '!':
            return 0;
    }
    return 0;
}

/* 
 * static unsigned int op_arg_count(const char c)
 * {
 *     switch(c)  {
 *         case '*': case '/': case '%': case '+': case '-': case '=': case '&': case '|':
 *             return 2;
 *         case '!':
 *             return 1;
 *         default:
 *             return c - 'A';
 *     }
 *     return 0;
 * }
 */

#define is_operator(c)  (c == '+' || c == '-' || c == '/' || c == '*' || c == '!' || c == '%' || c == '=' || c == '&' || c == '|')
#define is_function(c)  (c >= 'A' && c <= 'Z')
#define is_ident(c)     ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))

static int shunting_yard(const char *input, char *output)
{
    const char *strpos = input, *strend = input + strlen(input);
    char c, *outpos = output;

    char stack[32];       // operator stack
    unsigned int sl = 0;  // stack length
    char     sc;          // used for record stack element


    while(strpos < strend)   {
        // read one token from the input stream
        c = *strpos;
        if(c != ' ')    {

            // If the token is a number (identifier), then add it to the output queue.
            if(is_ident(c))  {

                *outpos = c; ++outpos;
            }
            // If the token is a function token, then push it onto the stack.
            else if(is_function(c))   {
                stack[sl] = c;
                ++sl;
            }
            // If the token is a function argument separator (e.g., a comma):
            else if(c == ',')   {
                int pe = 0;
                while(sl > 0)   {
                    sc = stack[sl - 1];
                    if(sc == '(')  {
                        pe = 1;
                        break;
                    }
                    else  {
                        // Until the token at the top of the stack is a left parenthesis,
                        // pop operators off the stack onto the output queue.
                        *outpos = sc;
                        ++outpos;
                        sl--;
                    }
                }
                // If no left parentheses are encountered, either the separator was misplaced
                // or parentheses were mismatched.
                if(!pe)   {
                    printf("Error: separator or parentheses mismatched\n");
                    return 0;
                }
            }
            // If the token is an operator, op1, then:
            else if(is_operator(c))  {
                while(sl > 0)    {
                    sc = stack[sl - 1];
                    if(is_operator(sc) &&
                        ((op_left_assoc(c) && (op_preced(c) >= op_preced(sc))) ||
                           (op_preced(c) > op_preced(sc))))   {
                        // Pop op2 off the stack, onto the output queue;
                        *outpos = sc;
                        ++outpos;
                        sl--;
                    }
                    else   {
                        break;
                    }
                }
                // push op1 onto the stack.
                stack[sl] = c;
                ++sl;

            }
            // If the token is a left parenthesis, then push it onto the stack.
            else if(c == '(')   {
                stack[sl] = c;
                ++sl;
            }
            // If the token is a right parenthesis:
            else if(c == ')')    {
                int pe = 0;
                // Until the token at the top of the stack is a left parenthesis,
                // pop operators off the stack onto the output queue
                while(sl > 0)     {
                    sc = stack[sl - 1];
                    if(sc == '(')    {
                        pe = 1;
                        break;
                    }
                    else  {
                        *outpos = sc;
                        ++outpos;
                        sl--;
                    }
                }
                // If the stack runs out without finding a left parenthesis, then there are mismatched parentheses.
                if(!pe)  {
                    printf("Error: parentheses mismatched\n");
                    return 0;
                }
                // Pop the left parenthesis from the stack, but not onto the output queue.
                sl--;
                // If the token at the top of the stack is a function token, pop it onto the output queue.
                if(sl > 0)   {
                    sc = stack[sl - 1];
                    if(is_function(sc))   {
                        *outpos = sc;
                        ++outpos;
                        sl--;
                    }
                }
            }
            else  {
                printf("Unknown token %c\n", c);
                return 0; // Unknown token
            }
        }
        ++strpos;
    }

    // When there are no more tokens to read:
    // While there are still operator tokens in the stack:

    while(sl > 0)  {
        sc = stack[sl - 1];
        if(sc == '(' || sc == ')')   {
            printf("Error: parentheses mismatched\n");
            return 0;
        }
        *outpos = sc;
        ++outpos;
        --sl;
    }
    *outpos = 0; // Null terminator
    return 1;
}

int bdd_eval_bool(char * expr)  {
    char output[500] = {0};
    char * op;
    int tmp;
    char part1[250], part2[250];

    if(!shunting_yard(expr, output)) {
      vector_error("bdd_eval_bool: incorrect expression: %s",expr);
      return 0;  // oops can't convert to postfix form
    }

    while (strlen(output) > 1) {
        op = &output[0];

        while (!is_operator(*op) && *op != '\0')
          op++;
        if (*op == '\0') {
          return 0;  // oops - zero operators found
        }
        else if (*op == '!') {
            tmp = !(*(op-1) - 48);
            *(op-1) = '\0';
        }
        else if(*op == '&') {
            tmp = (*(op-1) - 48) && (*(op-2) - 48);
            *(op-2) = '\0';
        }
        else if (*op == '|') {
            tmp = (*(op-1) - 48) || (*(op-2) - 48);
            *(op-2) = '\0';
        }

        memset(part1, 0, sizeof(part1));

        memset(part2, 0, sizeof(part2));
        strcpy(part1, output);

        strcpy(part2, op+1);

        memset(output, 0, sizeof(output));
        strcat(output, part1);

        strcat(output, ((tmp==0) ? "0" : "1"));

        strcat(output, part2);

    }
    int res = (*output - 48);
    // fprintf(stdout,"EVAL(%s)=%d\n",expr,res);
    return res;
}

#ifdef TEST_CONFIG
#endif
