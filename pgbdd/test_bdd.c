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

#include "bdd.c"

static bdd_dictionary* get_test_dictionary(char* dict_vars) {
    char* dictname = "Test";
    bdd_dictionary dict_struct, *new_dict;
    char* _errmsg = NULL;

    if ( ! (new_dict = bdd_dictionary_create(&dict_struct,dictname)) )
        pg_fatal("unable to create dictionary %s",dictname);
    if ( ! modify_dictionary(new_dict,DICT_ADD,dict_vars,&_errmsg))
        pg_fatal("error loding dictionary: %s",_errmsg);
    bdd_dictionary* res = bdd_dictionary_serialize(new_dict);
    bdd_dictionary_free(new_dict);
    bdd_dictionary_sort(res);
    return res;
}

static bdd* get_test_bdd(char* expr, int verbose) {
    char* _errmsg = NULL;
    bdd*  res;

    if ( !(res = create_bdd(BDD_BASE,expr,&_errmsg,verbose)) )
        pg_fatal("bdd create failed: %s",_errmsg);
    return res;
}

char *bdd_expr[] = {
    "(x=1) & y=2",
    "(x=1 | x=2)&(y=1 | y=1) | (z=4)",
    "x=1 & (x=2 | x=3 & x=4) | (x=5 & x=6 | x=7 & x=8) | y=4",
    "x=1 | (y=1 & x=2)",
    0
};

#include <time.h>

static void test_timings(){
    bdd* test_bdd;
    char* _errmsg = NULL;

    int REPEAT = 10000;
    int count  = 0;
    clock_t start = clock(), diff;
    for (int r=0; r<REPEAT; r++) {
        for (int i=0; bdd_expr[i]; i++) {
            // fprintf(stderr,"EXPR: %s\n",bdd_expr[i]);
            if ( (test_bdd = create_bdd(BDD_BASE,bdd_expr[i],&_errmsg,0)) ) {
                count++;
                FREE(test_bdd);
            } else {
                fprintf(stderr,"test_timings:error: %s\n",(_errmsg ? _errmsg : "NULL"));
                exit(0);
            }
        }
    }
    diff = clock() - start;
    int msec = diff * 1000 / CLOCKS_PER_SEC;
    fprintf(stdout,"Time %ds/%dms, created %d bdd's (%d/s)\n", msec/1000, msec%1000,count, (int)((double)count/((double)msec/1000)));
}

//
//
//

static void test_bdd_creation(){
    // char* expr = "((x=1 and x=1) or (not y=0 and not zzz=1)) and((xx=2 and x=3)or(not x=2 and not x=3))";
    // char* expr = "((x=1 | x=2) & x=2)";
    // char* expr = "   x=1 & x=2 | x=3 & x=4 | x=5 & x=6 | x=7 & x=8 | y=4";
    // (x0 and y1) or (not x0 and not y1) or ((x2 and y2) and y34)
    // char* expr = "(x=1 | x=2)&(y=1 | y=1) | (z=4)";
    // char* expr = "x=1 | (y=1 & x=2)";
    // char* expr = "(x=1 | x=2)";
    // char* expr = "(x=4 | x=2 | x=3 | x=1) & (y=2 | y=1 | y=3 )";
    char* expr = "(x=1 | x=2 | x=3 | x=4)";
    bdd* test_bdd;
    char* _errmsg = NULL;

    if ( (test_bdd = create_bdd(BDD_BASE,expr,&_errmsg,1/*verbose*/)) ) {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        char* dot_filename = "./DOT/test.dot";

        bdd_info(test_bdd,pbuff);
        pbuff_flush(pbuff,stdout);
        if ( dot_filename ) {
            FILE* f = fopen(dot_filename,"w");

            if ( ! f )
                fprintf(stderr,"Warning: unable to write dotfile: %s\n",dot_filename);
            bdd_generate_dot(test_bdd,pbuff,NULL);
            pbuff_flush(pbuff,f);
            fclose(f);
            pbuff_free(pbuff);
            fprintf(stdout,"Generated dotfile: %s\n",dot_filename); 
        }
        FREE(test_bdd);
    } else {
        fprintf(stderr,"Test_bdd:error: %s\n",(_errmsg ? _errmsg : "NULL"));
    }
}

//
//
//

#define EXTRA_PRINT_SIZE 32

static double bdd_prob_test(char* expr, char* dict_vars, char* dotfile, int verbose, int show_prob_in_dot) {
    bdd_dictionary *dict = get_test_dictionary(dict_vars);;
    bdd            *pbdd = get_test_bdd(expr,0/*verbose*/);

    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    if ( verbose )
        bdd_info(pbdd,pbuff); pbuff_flush(pbuff,stdout);

    char** extra = NULL;
    if ( dotfile && show_prob_in_dot ) {
        int n = V_rva_node_size(&pbdd->tree);
        int sz_ptrs = n * sizeof(char*);
        extra = (char**)MALLOC(sz_ptrs + (n * EXTRA_PRINT_SIZE));
        char* extra_str_base = ((char*)extra)+sz_ptrs;
        for(int i=0; i<n; i++) {
            extra[i]    = extra_str_base + i*EXTRA_PRINT_SIZE;
            *(extra[i]) = 0;
        }
    }
    char* _errmsg;
    double prob = bdd_probability(dict,pbdd,extra,verbose,&_errmsg);
    if ( prob < 0.0 )
        pg_fatal("test_bdd_probability: %s",_errmsg);
    else {
        if ( verbose ) 
            fprintf(stdout,"+ P[%s]=%f\n",expr,prob);
    }
    if ( dotfile ) {
        bdd_generate_dotfile(pbdd,dotfile,extra);
        if ( verbose )
            fprintf(stdout,"bdd_prob_test: generated dot file %s in \"%s\".\n",(extra ? "with probabilities":""),dotfile);
 
    }
    if ( extra )
        FREE(extra);
    bdd_dictionary_free(dict);
    FREE(dict);
    FREE(pbdd);
    pbuff_free(pbuff);
    //
    return prob;
}

static int test_bdd_probability() {
    bdd_prob_test(
            // "(x=1|x=2)",
            "( x  = 1 | y =         1)",
            "x=1:0.4; x=2:0.6 ; y=1:0.2; y=2:0.8; ",
            "./DOT/test.dot", /* filename of dotfile or NULL */
            1 /* verbose */,
            1 /* show probabilities in dotfile */
       );
    return 1;
}

//
//
//

void test_bdd() {
    if (0) test_bdd_creation();
    if (1) test_bdd_probability();
    if (0) test_timings();
}
