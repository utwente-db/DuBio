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
#include "bdd.h"
#include "utils.h"

#define is_rva_char(C) (isalnum(C)||C=='=')

bddstr create_bddstr(char* val, int len) {
    bddstr newstr;

    if ( len >= MAXRVA )
        vector_error("RVA string too large");
    memcpy(newstr.str,val,len);
    newstr.str[len] = 0;
    return newstr;
}

void bdd_print_V_bddstr(V_bddstr* v, FILE* f) {
    fprintf(f,"{");
    for(int i=0; i<V_bddstr_size(v); i++) {
        fprintf(f,"%s ",V_bddstr_get(v,i).str);
    }
    fprintf(f,"}");
}

V_bddstr bdd_set_default_order(char* expr) {
    V_bddstr res;

    V_bddstr_init(&res);
    char*p = expr;
    do {
        while (*p && !is_rva_char(*p)) p++;
        if ( *p) {
            char* start=p; 

            while (*p && is_rva_char(*p)) p++; 
            bddstr s = create_bddstr(start,p-start);
            if ( !(strcmp(s.str,"not")==0||strcmp(s.str,"and")==0||strcmp(s.str,"or")==0) )
                V_bddstr_add(&res,s);
        }
    } while (*p);
    // now sort the result string and make result unique
    if ( V_bddstr_size(&res) > 0) {
        V_bddstr_quicksort(&res,0,res.size-1,cmpBddstr);
        bddstr last = V_bddstr_get(&res,0);
        for(int i=1; i<V_bddstr_size(&res); i++) {
            bddstr curstr = V_bddstr_get(&res,i);
            if ( cmpBddstr(last,curstr)==0 )
                V_bddstr_delete(&res,i--); // remove i'th element and decrease i
            last = curstr;
        }
    }
    return res;
}

void bdd_print_row(bddrow row, FILE* f) {
        fprintf(f,"[\"%s\",%d,%d]\n",row.rva,row.low,row.high);
}

void bdd_print_tree(V_bddrow* tree, FILE* f) {
    fprintf(f,"Tree:\n");
    fprintf(f,"-----\n");
    for(int i=0; i<V_bddrow_size(tree); i++) {
        fprintf(f,"%d:\t",i);
        bdd_print_row(V_bddrow_get(tree,i),f);
    }
}

void bdd_generate_dot(bdd* bdd, char* filename) {
    FILE* f;
    V_bddrow* tree = &bdd->tree;

    if ( !(f = fopen(filename,"w") ) )
       vector_error("bdd_generate_dot: cannot open \"%s\" for write",filename);
    fprintf(f,"digraph {\n");
    for(int i=0; i<V_bddrow_size(tree); i++) {
        bddrow row = V_bddrow_get(tree,i);
        if ( i<2 ) {
            fprintf(f,"\tnode [shape=square]\n");
            fprintf(f,"\t%d [label=\"%s\"]\n",i,row.rva);
        } else {
            fprintf(f,"\tnode [shape=circle]\n");
            fprintf(f,"\t%d [label=\"%s\"]\n",i,row.rva);
            fprintf(f,"\tedge [shape=rarrow style=dashed]\n");
            fprintf(f,"\t%d -> %d\n",i,row.low);
            fprintf(f,"\tedge [shape=rarrow style=bold]\n");
            fprintf(f,"\t%d -> %d\n",i,row.high);
        }
    }
    fprintf(f,"}\n");
}

/*
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
    // fprintf(stdout,"+REPLACE-RES: \"%s\" {%s->%s}> \t\"%s\"\n",src,find,replace,dst);
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
        return BDD_NONE;
}

double bdd_atof(char* a) {
    char * endptr;
    double d = strtod(a, &endptr);
    if (a == endptr)
        return (double)BDD_NONE;
    else
        return d;
}

/*
 *
 *
 */

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

#define bool int
#define false 0
#define true 1

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

static bool op_left_assoc(const char c)
{
    switch(c)    {
        // left to right
        case '*': case '/': case '%': case '+': case '-': case '|': case '&':
            return true;
        // right to left
        case '=': case '!':
            return false;
    }
    return false;
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

static bool shunting_yard(const char *input, char *output)
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
                bool pe = false;
                while(sl > 0)   {
                    sc = stack[sl - 1];
                    if(sc == '(')  {
                        pe = true;
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
                    return false;
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
                bool pe = false;
                // Until the token at the top of the stack is a left parenthesis,
                // pop operators off the stack onto the output queue
                while(sl > 0)     {
                    sc = stack[sl - 1];
                    if(sc == '(')    {
                        pe = true;
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
                    return false;
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
                return false; // Unknown token
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
            return false;
        }
        *outpos = sc;
        ++outpos;
        --sl;
    }
    *outpos = 0; // Null terminator
    return true;
}

bool bdd_eval_bool(char * expr)  {
    char output[500] = {0};
    char * op;
    bool tmp;
    char part1[250], part2[250];

    if(!shunting_yard(expr, output)) {
      vector_error("bdd_eval_bool: incorrect expression: %s",expr);
      return false;  // oops can't convert to postfix form
    }

    while (strlen(output) > 1) {
        op = &output[0];

        while (!is_operator(*op) && *op != '\0')
          op++;
        if (*op == '\0') {
          return false;  // oops - zero operators found
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

        strcat(output, ((tmp==false) ? "0" : "1"));

        strcat(output, part2);

    }
    bool res = (*output - 48);
    // fprintf(stdout,"EVAL(%s)=%d\n",expr,res);
    return res;
}

#ifdef TEST_CONFIG
#endif
