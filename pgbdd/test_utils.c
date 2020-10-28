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

#include "test_config.h"

#include "utils.c"

static int test_pbuff(){
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    for(int i=0; i<1000000; i++)
        bprintf(pbuff," %d ",i);
    fprintf(stderr,"+ buffsize=%ld\n",strlen(pbuff->buffer));
    pbuff_reset(pbuff);
    bprintf(pbuff,"This %s a %s ","is","test");
    pbuff_flush(pbuff,stderr);
    bprintf(pbuff,"of the %s pbuffer %s\n","new","class");
    pbuff_flush(pbuff,stderr);
    pbuff_free(pbuff);
    return 1;
}

//
//
//

#ifdef BEE_DEBUG

static void bee_create_dot(char* filename) {
    FILE* f = fopen(filename,"w");
    if ( f ) {
         fprintf(f,"digraph {\n");
         fprintf(f,"\tlabelloc=\"t\";\n");
         fprintf(f,"\tlabel=\"Boolean Expression Evaluator (BEE) Graph. (C) J.Flokstra\"");
         for(int i=0; i<bee_NSTATES; i++) {
             char* statename = bee_state_STR[i];
             switch (i) {
              case bee_s0:
              case bee_s1:
              case bee_snot:
              case bee_s0and:
              case bee_s0or:
              case bee_s1and:
              case bee_s1or:
              case bee_salways0:
              case bee_salways1:
              case bee_svalstart:
                 fprintf(f,"\t%s [shape=oval,label=%s]\n",statename,statename);
                 break;
              case bee_sresult0:
                 fprintf(f,"\t%s [shape=rarrow,label=%s]\n",statename,"<<b>0</b>>");
                 break;
              case bee_sresult1:
                 fprintf(f,"\t%s [shape=rarrow,label=%s]\n",statename,"<<b>1</b>>");
                 break;
              case bee_sparopen:
                 fprintf(f,"\t%s [shape=tripleoctagon,label=%s]\n",statename,statename);
                 fprintf(f,"\tedge [shape=rarrow style=dotted arrowtail=dot]\n");
                 fprintf(f,"\t%s -> %s [label=<<b>\'%s\'</b>>]\n",statename,"SVALSTART","PUSH(prev_state)");
                 break;
              case bee_sparclose:
                 fprintf(f,"\t%s [shape=tripleoctagon,label=%s]\n",statename,statename);
                 break;
             };
             //
             if ( i < bee_NSTATES_STATIC ) {
                 for (int t=0; t<bee_NTOKEN; t++) {
                     if ( fsm[i][t] != bee_error ) {
                         fprintf(f,"\tedge [shape=rarrow style=dashed arrowtail=dot]\n");
                         fprintf(f,"\t%s -> %s [label=<<b>\'%s\'</b>>]\n",statename,bee_state_STR[fsm[i][t]],bee_token_STR[t]);
                     }
                 }
             }

         }
        fprintf(f,"\t%s [shape=rarrow,label=%s]\n","START","<<b>START</b>>");
        fprintf(f,"\tedge [shape=rarrow style=dotted arrowtail=dot]\n");
        fprintf(f,"\t%s -> %s [label=<<b>\'%s\'</b>>]\n","START","SVALSTART","expression");
        for(int i=0; i<bee_NSTATES_STATIC; i++) {
            if (fsm[i][bee_paropen] == bee_sparopen) {
                fprintf(f,"\tedge [shape=rarrow style=dotted arrowtail=dot]\n");
                fprintf(f,"\t%s -> %s [label=<<b>\'%s\'</b>>]\n","SPARCLOSE",bee_state_STR[i],"POP()[T(prev_state)]");
            }
        }
        fprintf(f,"}\n");
        fclose(f);
    }
}

#endif

typedef struct bee_test {
    int  r;
    char q[256];
} bee_test;

bee_test testset[] = {
 { .r=0, .q="0" },
 { .r=1, .q="1" },
 { .r=1, .q="1&1&1" },
 { .r=1, .q="0&1|1" },
 { .r=1, .q="!0" },
 { .r=1, .q="!!1" },
 { .r=1, .q="0&!0|1" },
 { .r=1, .q="1&1" },
 { .r=0, .q="(1&0)" },
 { .r=1, .q="(0|!!1)" },
 { .r=1, .q="(0|!!((1)))" },
 { .r=1, .q="(1|!!((1)&1))" },
 { .r=1, .q="(1|!!((1&1|0)&1))" },
 { .r=1, .q="(1|!!((1&1|0)&1))" },
 //
 { .r=1, .q="1" }
};

static void run_bee_testset() {
    char testbuff[256];
    char *_errmsg=NULL;

    for (int i=0; i<sizeof(testset)/sizeof(bee_test); i++) {
        strcpy(testbuff,testset[i].q);
        fprintf(stderr,"Test: \"%s\"\n",testset[i].q);
        int res = bee_eval(testbuff,&_errmsg);
        if ( res != testset[i].r ) {
            if ( res < 1 ) {
                fprintf(stderr,"Error: %s",(_errmsg)?_errmsg:"none");
                exit(0);
            } else
                fprintf(stderr,"Error[%s]: expected(%d)\n",testset[i].q,testset[i].r);
            exit(0);
        }
    }
}

//
//
//

#define VAL_STAR -9999

typedef enum dict_mode{DICT_ADD, DICT_DEL, DICT_UPD} dict_mode;

static int parse_dictionary(dict_mode mode, char* dictionary_def, char** _errmsg) {
    char *p = dictionary_def;
    while ( *p ) {
        char*  scan_var     = NULL;
        int    scan_var_len = -1;
        int    scan_val     = -1;
        double scan_prob    = -1;
        while ( *p && !isalnum(*p) )
            p++;
        if ( isalnum(*p) ) {
            scan_var = p;
            while ( isalnum(*p) )
                p++;
            scan_var_len = p - scan_var;
            while ( *p && *p != '=' )
                p++;
            if ( !(*p++ == '=') )
                return pg_error(_errmsg,"parse_dict: missing \'=\' in dictionay def: \"%s\"",scan_var);

            while ( isspace(*p) ) p++;
            if ( *p == '*' ) {
                scan_val = VAL_STAR;
            } else if ( isdigit(*p) ) {
                if ( (scan_val = bdd_atoi(p)) < 0 )
                    return pg_error(_errmsg,"parse_dict: bad integer value : \"%s\"",p);
            } else 
                return pg_error(_errmsg,"parse_dict: missing value after \'=\' in expr: \"%s\"",scan_var);

            p++;
            while (isdigit(*p) )
                p++;
            // 
            scan_prob = -1;
            if ( mode != DICT_DEL ) {
                while ( isspace(*p) ) p++;
                if ( !(*p++ == ':') )
                    return pg_error(_errmsg,"parse_dict: missing \':\' in dictionay def: \"%s\"",scan_var);
                while ( isspace(*p) ) p++;
                char * endptr;
                scan_prob = strtod(p, &endptr);
                if ( p == endptr )
                    return pg_error(_errmsg,"parse_dict: bad probability value : \"%s\"",p);
                p = endptr;
            }
            while ( isspace(*p) ) p++;
            if ( *p == ';' ) {
                p++;
                while ( isspace(*p) ) p++;
            }
        }
        // 
        char buff[32];
        memcpy(buff,scan_var,scan_var_len);
        buff[scan_var_len] = 0;
        fprintf(stderr,"SCANNED: var=%s, val=%d, prob=%f\n",buff,scan_val,scan_prob);
    }
    return 1;
}

void test_utils() {
#ifdef BEE_DEBUG
    bee_create_dot("./DOT/bee.dot");
#endif
    char* _errmsg;
    if ( !parse_dictionary(DICT_ADD, "x=1:0.1;   y=4:100; q=5:0.4 ", &_errmsg) )
        fprintf(stderr,"ERROR: %s\n",_errmsg);
    //
    if ( 0 ) test_pbuff();
    if ( 0 ) run_bee_testset();
}
