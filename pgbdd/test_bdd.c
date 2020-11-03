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

#define BDD_VERBOSE

#include "bdd.c"

static bdd_dictionary* get_test_dictionary(char* dict_vars, char** _errmsg) {
    bdd_dictionary dict_struct, *new_dict;

    if ( ! (new_dict = bdd_dictionary_create(&dict_struct)) ) {
        pg_error(_errmsg,"get_test_dictionary: error creating dictionary");
        return NULL;
    }
    if ( ! modify_dictionary(new_dict,DICT_ADD,dict_vars,_errmsg))
        return NULL;
    bdd_dictionary* res = dictionary_prepare2store(new_dict);
    return res;
}

static bdd* get_test_bdd(char* expr, int verbose, char** _errmsg) {
    return create_bdd(BDD_DEFAULT,expr,_errmsg,verbose);
}

char *bdd_expr[] = {
    "0",
    "1",
    "x=1",
    "(x=1|y=1)",
    "(x=1&y=1)|(z=5)",
    "x=1|(y=1 & x=2)",
    "(z=1)&!((x=1)&((y=1|y=2)&x=2))",
    "(x=8|x=2|x=4|x=3|x=9|(x=6|q=4&p=6)|x=7|x=1|x=5)",
    "(x=1&x=2|x=3&x=4|x=5&x=6|x=7&x=8|y=4)",
    0
};

static int _regenerate_test(char* bdd_expr[], char** _errmsg) {
    pbuff pbgen_struct, *pbgen=pbuff_init(&pbgen_struct);
    for (int i=0; bdd_expr[i]; i++) {
        bdd *bdd1, *bdd2;

        pbgen=pbuff_init(&pbgen_struct);
        // fprintf(stdout,"TESTING: %s\n",bdd_expr[i]);
        if ( !(bdd1 = create_bdd(BDD_DEFAULT,bdd_expr[i],_errmsg,0)) )
            return 0;
        bdd2string(pbgen,bdd1,0);
        char* bdd1_str = pbuff_preserve_or_alloc(pbgen);
        // fprintf(stdout,"TESTING: bdd1 = [%s]\n",bdd1_str);
        //
        pbgen=pbuff_init(&pbgen_struct);
        if ( !(bdd2 = create_bdd(BDD_DEFAULT,bdd1_str,_errmsg,0)) )
            return 0;
        bdd2string(pbgen,bdd2,0);
        char* bdd2_str = pbuff_preserve_or_alloc(pbgen);
        // fprintf(stdout,"TESTING: bdd2 = [%s]\n",bdd2_str);
        //
        if ( strcmp(bdd1_str,bdd2_str) != 0 ) 
            return pg_error(_errmsg,"regenerated string does not match original: %s <<<>>> %s", bdd1_str,bdd2_str);
        FREE(bdd1_str);
        FREE(bdd2_str);
    }
    return 1;
}

static int test_regenerate() {
    char* _errmsg;

    if ( !_regenerate_test(bdd_expr,&_errmsg) )
        pg_fatal("regenerate_test:error: %s",_errmsg);
    return 1;
}


#include <time.h>

static void test_timings(){
    int REPEAT = 10000;
    int n_bdd  = (sizeof(bdd_expr)/sizeof(char*)) - 1;
    int count  = REPEAT * n_bdd;
    clock_t start = clock(), diff;
    for (int r=0; r<REPEAT; r++) {
        test_regenerate();
    }
    diff = clock() - start;
    int msec = diff * 1000 / CLOCKS_PER_SEC;
    fprintf(stdout,"Time %ds/%dms, created %d bdd's (%d/s)\n", msec/1000, msec%1000,count, (int)((double)count/((double)msec/1000)));
}

static char* TRIO_DICTIONARY = 
                "d1=1:0.800000; d1=2:0.200000;"
                "d2=1:0.700000; d2=2:0.300000;"
                "d3=1:0.500000; d3=2:0.500000;"
                "s1=1:0.700000; s1=2:0.200000; s1=3:0.100000;"
                "s2=1:0.250000; s2=2:0.250000; s2=3:0.250000; s2=4:0.250000;"
                "s3=1:0.600000; s3=2:0.400000;"
                "s4=1:0.900000; s4=2:0.100000;";

char *trio_expr[] = {
        "(d1=1 & s4=1)",
        "(d2=1 & s1=1)",
        "(d1=1 & s1=2)",
        "(d3=1 & s2=1)",
        "(d3=2 & s2=1)",
        "(d1=2 & s4=2)",
        0
};

static int test_trio() {
    char* _errmsg = NULL;
    bdd_dictionary* dict;
    bdd* pbdd;

    if ( ! (dict = get_test_dictionary(TRIO_DICTIONARY,&_errmsg)))
        pg_fatal("test_trio: error creating dictionary: %s",_errmsg);
    double total = 0.0;
    for (int i=0; trio_expr[i]; i++) {
        if ( !(pbdd = get_test_bdd(trio_expr[i],0/*verbose*/,&_errmsg)))
            pg_fatal("test_trio: error creating bdd: %s",_errmsg);
        double prob = bdd_probability(dict,pbdd,NULL,0,&_errmsg);
        if ( prob < 0.0 )
            pg_fatal("test_trio: error computing prob: %s",_errmsg);
        // fprintf(stderr,"+ PROB[%s] = %f\n",trio_expr[i],prob);
        total += prob;
    }
    if ( (int)(total*100) != 164 )
        pg_fatal("test_trio:assert: total changed");
    return 1;
}

//
//
//


static void test_bdd_creation(){
    // char* expr = "(x=1 & y=1 & z=1 )";
    // char* expr = "(x=1&y=1) |(z=5)";
    char* expr = "(z=1)&!((x=1)&((y=1|y=2)&x=2))";
    bdd*  test_bdd;
    char* _errmsg = NULL;

    if ( (test_bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,1/*verbose*/)) ) {
        pbuff pbuff_struct, *pb=pbuff_init(&pbuff_struct);
        char* dot_filename = "./DOT/test.dot";

        bdd_info(test_bdd,pb);
        pbuff_flush(pb,stdout);
        pbuff pbgen_struct, *pbgen=pbuff_init(&pbgen_struct);
        bdd2string(pbgen,test_bdd,0);
        char* regen_str = pbuff_preserve_or_alloc(pbgen);
        fprintf(stdout,"BDD2STRING = [%s]\n",regen_str); 
        //
        if ( dot_filename ) {
            FILE* f = fopen(dot_filename,"w");

            if ( ! f )
                fprintf(stderr,"Warning: unable to write dotfile: %s\n",dot_filename);
            else {
                bdd_generate_dot(test_bdd,pb,NULL);
                pbuff_flush(pb,f);
                fclose(f);
                pbuff_free(pb);
                fprintf(stdout,"Generated dotfile: %s\n",dot_filename); 
            }
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

static double bdd_prob_test(char* expr, char* dict_vars, char* dotfile, int verbose, int show_prob_in_dot, char** _errmsg) {
    bdd_dictionary *dict = get_test_dictionary(dict_vars,_errmsg);
    bdd            *pbdd = get_test_bdd(expr,0/*verbose*/,_errmsg);

    if ( !dict || !pbdd )
        return -1.0;
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
    double prob = bdd_probability(dict,pbdd,extra,verbose,_errmsg);
    if ( prob < 0.0 )
        return -1.0;
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
    char* _errmsg;
    if (
        bdd_prob_test(
            // "(x=1|x=2)",
            "( x=1 | y=1)",
            // "(x=1&((y=1|y=2)&x=2))",
            "x=1:0.4; x=2:0.6 ; y=1:0.2; y=2:0.8; ",
            "./DOT/test.dot", /* filename of dotfile or NULL */
            1 /* verbose */,
            1 /* show probabilities in dotfile */,
            &_errmsg
            ) < 0.0 ) {
        fprintf(stderr,"test_bdd_probability: error: %s",_errmsg);
        return 0;
    } else
        return 1;
}

//
//
//


static rva_node stree[] = {
        {.rva={.var="0",.val=-1}, .low=-1, .high=-1},
        {.rva={.var="1",.val=-1}, .low=-1, .high=-1},
        {.rva={.var="x",.val= 9}, .low=-1, .high=-1}
};

static bdd* create_static_bdd(rva_node sn[], int n) {
    bdd bdd_struct, *res = &bdd_struct;

    V_rva_node_init(&res->tree);
    for(int i=0; i<n; i++) {
        V_rva_node_add(&res->tree,&sn[i]);
    }
    return res;
}

static int test_static_bdd() {
    create_static_bdd(stree,3);
    return 1;
}


//
//
//

void test_bdd() {
    if (1) test_regenerate(); // do this always, catches many errors
    if (1) test_trio();       // ,,
    //
    if (0) test_bdd_creation();
    if (1) test_bdd_probability();
    if (0) test_timings();
    if (0) test_static_bdd();
}
