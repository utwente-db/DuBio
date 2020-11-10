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

#ifndef BDD_OPTIMIZE
#define BDD_VERBOSE
#endif

#include "bdd.c"

#include <time.h>
static clock_t _clock_start, _clock_stop;
#define CLOCK_START() _clock_start = clock()
#define CLOCK_STOP()  _clock_stop  = clock()
#define CLOCK_MS()    ((_clock_stop-_clock_start)*1000/CLOCKS_PER_SEC)


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

static int _regenerate_test(char* par_expr, char** _errmsg) {
    pbuff pbgen_struct, *pbgen=pbuff_init(&pbgen_struct);
    bdd *bdd1, *bdd2;

    int VERBOSE = 0;

    if (VERBOSE) fprintf(stdout,"TESTING: %s\n",par_expr);
    if ( !(bdd1 = create_bdd(BDD_DEFAULT,par_expr,_errmsg,0)) )
        return 0;
    bdd2string(pbgen,bdd1,0);
    char* bdd1_str = pbuff_preserve_or_alloc(pbgen);
    if (VERBOSE) fprintf(stdout,"TESTING: bdd1 = [%s]\n",bdd1_str);
    //
    pbgen=pbuff_init(&pbgen_struct);
    if ( !(bdd2 = create_bdd(BDD_DEFAULT,bdd1_str,_errmsg,0)) )
        return 0;
    bdd2string(pbgen,bdd2,0);
    char* bdd2_str = pbuff_preserve_or_alloc(pbgen);
    if (VERBOSE) fprintf(stdout,"TESTING: bdd2 = [%s]\n",bdd2_str);
    //
    if ( strcmp(bdd1_str,bdd2_str) != 0 ) {
        int res_eq = bdd_test_equivalence(bdd1_str,bdd1_str,_errmsg);
        if ( res_eq < 0 )
            pg_fatal("regenerate_test:equivalence error: %s",_errmsg);
        if ( !res_eq ) {
            if ( 1 ) {
                fprintf(stdout,"/--------------------------------------------\n");
                fprintf(stdout,"1> %s\n",bdd1_str);
                fprintf(stdout,"/--------------------------------------------\n");
                fprintf(stdout,"2> %s\n",bdd2_str);
                fprintf(stdout,"/--------------------------------------------\n");
            }
            return pg_error(_errmsg,"regenerated string not equivalent to original: %s <<<>>> %s", bdd1_str,bdd2_str);
        }
    }
    FREE(bdd1);
    FREE(bdd2);
    FREE(bdd1_str);
    FREE(bdd2_str);
    //
    return 1;
}

static int test_regenerate() {
    char* _errmsg;

    for (int i=0; bdd_expr[i]; i++) {
        if ( !_regenerate_test(bdd_expr[i],&_errmsg) )
            pg_fatal("regenerate_test:error: %s",_errmsg);
    }
    return 1;
}


static void test_timings(){
    int REPEAT = 10000;
    int n_bdd  = (sizeof(bdd_expr)/sizeof(char*)) - 1;
    int count  = REPEAT * n_bdd;
    CLOCK_START();
    for (int r=0; r<REPEAT; r++) {
        test_regenerate();
    }
    CLOCK_STOP();
    int msec = CLOCK_MS();
    fprintf(stdout,"+ Time %ds/%dms\n",msec/1000,msec%1000);
    fprintf(stdout,"+ Created %d bdd's (%d/s)\n",count, (int)((double)count/((double)msec/1000)));
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

#define N_LHS 3
static char *lhs_expr[N_LHS] = {
"x=1|(!y=3)",
"(x=2&z=4)",
"x=3"
};
static bdd* lhs_bdd[N_LHS];

#define N_RHS 3
static char *rhs_expr[N_RHS] = {
"y=1",
"(y=2|x=1|x=2)",
"(y=3|z=4)"
};
static bdd* rhs_bdd[N_RHS];


static void run_apply_test(op_mode m) {
    char* _errmsg = NULL;

    int repeat = 100000;
    int count = 0;
    CLOCK_START();
    for(int r=0; r<repeat; r++) {
        for(int i=0; i<N_LHS; i++) {
            for(int j=0; j<N_LHS; j++) {
                bdd* res;
                if (!(res=bdd_operator('&',m,lhs_bdd[i],rhs_bdd[j],&_errmsg)))
                    pg_fatal("compare_apply_text: error: %s",_errmsg);
                FREE(res);
                count++;
                if (!(res=bdd_operator('|',m,lhs_bdd[i],rhs_bdd[j],&_errmsg)))
                    pg_fatal("compare_apply_text: error: %s",_errmsg);
                FREE(res);
                count++;
            }
        }
    }
    CLOCK_STOP();
    int msec = CLOCK_MS();
    fprintf(stdout,"+ result of \"%s\" test:\n",(m==BY_TEXT?"TEXT":"APPLY"));
    fprintf(stdout,"+ Time %ds/%dms, #op=%d, #op/s=%d\n",msec/1000,msec%1000,count,(int)((double)count/((double)msec/1000)));
} 

static int compare_apply_text() {
    char* _errmsg = NULL;

    for (int i=0; i<N_LHS; i++) 
        if (!(lhs_bdd[i] = create_bdd(BDD_DEFAULT,lhs_expr[i],&_errmsg,0)))
            pg_fatal("compare_apply_text: error: %s",_errmsg);
    for (int i=0; i<N_RHS; i++) 
        if (!(rhs_bdd[i] = create_bdd(BDD_DEFAULT,rhs_expr[i],&_errmsg,0)))
            pg_fatal("compare_apply_text: error: %s",_errmsg);
    //
    run_apply_test(BY_TEXT);
    run_apply_test(BY_APPLY);
    //
    return 1;
} 

static int test_apply() {
    char* _errmsg = NULL;
    bdd *lhs, *rhs;
    // char* dotfile = NULL;
    char* dotfile = "./DOT/test.dot";
  

    char* la = "(x=1&y=2)";
    char  op = '&';
    // char* ra = "y=2";
    char* ra = "(x=1&y=2&z=3)";

    lhs = create_bdd(BDD_DEFAULT,la,&_errmsg,0/*verbose*/);
    rhs = create_bdd(BDD_DEFAULT,ra,&_errmsg,0/*verbose*/);
    if ( !(lhs && rhs) )
        pg_fatal("test_apply: error: %s",_errmsg);
    bdd* res = bdd_apply(op,lhs,rhs,0/*verbose*/,&_errmsg);
    if ( !res ) {
        fprintf(stderr,"test_apply:error: %s\n",_errmsg);
    } else {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        bprintf(pbuff,"L: ");
        bdd2string(pbuff,lhs,0);
        bprintf(pbuff,"\nO: %c\n",op);
        bprintf(pbuff,"R: ");
        bdd2string(pbuff,rhs,0);
        bprintf(pbuff,"\nRESULT APPLY : ");
        bdd2string(pbuff,res,0);
        bprintf(pbuff,"\n");
        pbuff_flush(pbuff,stdout);
        if ( dotfile )
            bdd_generate_dotfile(res,dotfile,NULL);
        bdd2string(pbuff,res,0);
        FREE(res);
        if ( !(res=create_bdd(BDD_DEFAULT,pbuff->buffer,&_errmsg,0/*verbose*/)))
            pg_fatal("test_apply: error: %s",_errmsg);
        pbuff_reset(pbuff);
        bprintf(pbuff,"AND REPARSED : ");
        bdd2string(pbuff,res,0);
        bprintf(pbuff,"\n");
        //
        if ( !(res = bdd_operator(op,BY_TEXT,lhs,rhs,&_errmsg)))
            pg_fatal("test_apply: error: %s",_errmsg);
        bprintf(pbuff,"STRING CONCAT: ");
        bdd2string(pbuff,res,0);
        bprintf(pbuff,"\n");
        pbuff_flush(pbuff,stdout);
        FREE(res);
    }
    return 1;
}

//
//
//


static void test_bdd_creation(){
    // char* expr = "(x=1 & y=1 & z=1 )";
    // char* expr = "(x=1&y=1) |(z=5)";
    // char* expr = "(z=1)&!((x=1)&((y=1|y=2)&x=2))";
    // char* expr = "!(x=1)";
    // char* expr = "!(x=1&y=2&z=3)";
    // char* expr = "(!x=1)|y=2";
    // char* expr = "!(a=4 | (b=4 & !d=1))"; // NOTMODE example
    // char* expr = "(a=0&((b=0&(c=1&d=0))|c=0))"; // c=0 / H and L
    // char* expr = "((d=0|c=0|a=0)|(!c=1&b=2)|(d=0|!d=1)|(a=1))";
    // char* expr = "((!d=1&!a=4&b=4))";
    // char* expr = "(a=1|b=2|c=3|d=4|e=5|f=6|g=7|h=8)";
    // char* expr = "!(a=1|b=2|!(c=3|d=4|!e=5))";
    // char* expr = "(a=1|b=2|!(c=3|d=4|!e=5))";
    // char* expr = "a=1";
    // char* expr = "!a=1";
    // char* expr = "a=1 | b=2";
    // char* expr = "a=1 & b=2";
    // char* expr = "!(a=1 | b=2)";
    // char* expr = "!(a=1 & b=2)";
    // char* expr = "!(a=1 & b=2 & c=3)";
    // char* expr = "!(a=1 | b=2 | c=3)";
    // char* expr = "(x=1|y=1)";
    //
    // problem cluster -> first xlates to second ()(OK) but result ->
    //
    // char* expr = "((c=1&c=1&a=0)&(d=0|b=1|!b=0)&(d=2|d=3|a=0))";
    char* expr = "(a=0&(b=0&(c=1&d=0)|c=0))"; // xlate from above
    // char* expr = "(a=0&(b=0&(c=0|(c=1&d=0))|c=0))"; // xlate from xlate
    

    bdd*  test_bdd;
    char* _errmsg = NULL;

    if ( (test_bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,1/*verbose*/)) ) {
        pbuff pbuff_struct, *pb=pbuff_init(&pbuff_struct);
        char* dot_filename = "./DOT/test.dot";

        fprintf(stdout,"#EXPR: %s\n",expr);
        bdd_info(test_bdd,pb);
        pbuff_flush(pb,stdout);
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
        //
        if ( 1 ) {
            char* _errmsg;
            if ( !_regenerate_test(expr,&_errmsg) )
                exit(0);
                // pg_fatal("regenerate_test:error: %s",_errmsg);
        }
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

/* 
 *
 * Random expression generaor
 *
 */


typedef struct randexpr {
    int MAX_LEVELS;
    int MAX_CLUSTER;
    int MAX_CLUSTER_SIZE;
    int N_VARS;
    int N_VALS;
    int NOT_MODULO;
} randexpr;

static int randInRange(int min, int max)
{
  return min + (int)(((double)rand()/(double)RAND_MAX)*(double)(max-min+1));
}

static char* genvar(int n) {
    static char vbuff[16];
    int i = 0;
    
    do {
        vbuff[i++] = 'a' + (n%26);
        n /= 26;
    } while ( n > 0 );
    vbuff[i] = 0;
    return vbuff;
}

static char* rand_var(randexpr* conf) {
    return genvar(randInRange(0,conf->N_VARS));
}

static int rand_val(randexpr* conf) {
    return randInRange(0,conf->N_VALS);
}

static char* rand_and_or(randexpr* conf) {
    return (rand()%2) ? "&" : "|"; // 50 - 50 distribution
}

static char* rand_not(randexpr* conf) {
    return (rand()%conf->NOT_MODULO) ? "" : "!"; // 1 : NOT-MODULO chance
}

static void generate_level(randexpr* conf, pbuff* pbuff, int level);

static void generate_expression(randexpr* conf, pbuff* pbuff, int level) {
    if ( randInRange(0,conf->MAX_LEVELS-1) > level )
        generate_level(conf, pbuff,++level);
    else {
        bprintf(pbuff,rand_not(conf));
        bprintf(pbuff,"%s=%d",rand_var(conf),rand_val(conf));
    }
}

static void generate_cluster(randexpr* conf, pbuff* pbuff, int level) {
    int n_expr = randInRange(1,conf->MAX_CLUSTER_SIZE); 
    char* op = rand_and_or(conf);
    bprintf(pbuff,rand_not(conf));
    bprintf(pbuff,"(");
    for (int i=0; i<n_expr; i++) {
        if (i) bprintf(pbuff,op); 
        generate_expression(conf,pbuff,level);
    }
    bprintf(pbuff,")");
}

static void generate_level(randexpr* conf, pbuff* pbuff, int level) {
    int n_clusters = randInRange(1,conf->MAX_CLUSTER); 
    char* op = rand_and_or(conf);
    bprintf(pbuff,"(");
    for (int i=0; i<n_clusters; i++) {
        if (i) bprintf(pbuff,op); 
        generate_cluster(conf,pbuff,level);
    }
    bprintf(pbuff,")");
}

static char* random_expression(randexpr* conf, pbuff* pbuff) {
    pbuff_flush(pbuff,NULL);
    generate_level(conf,pbuff,0);
    return pbuff->buffer;
}

/*
 * static randexpr  RANDEXPR = {
 *     .MAX_LEVELS       =  3,
 *     .MAX_CLUSTER      =  5,
 *     .MAX_CLUSTER_SIZE =  4,
 *     .NOT_MODULO       =  6,
 *     .N_VARS           =  3,
 *     .N_VALS           =  4
 * };
 */

static randexpr  RANDEXPR = {
    .MAX_LEVELS       =  1,
    .MAX_CLUSTER      =  4,
    .MAX_CLUSTER_SIZE =  3,
    .NOT_MODULO       =  5,
    .N_VARS           =  3,
    .N_VALS           =  4
};



static void random_test(int n, long seed) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    srand(seed);
  
    for (int i=0; i<n; i++) {
        char* _errmsg = NULL;
        bdd*      bdd = NULL;
        char *expr    = random_expression(&RANDEXPR,pbuff);

        if (!(bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,0))) {
            pg_fatal("random_test: error: %s",_errmsg);
        }
        FREE(bdd);
        if ( 1 ) {
            char* _errmsg;
            if ( !_regenerate_test(expr,&_errmsg) ) {
                pg_fatal("regenerate_test:error: %s",_errmsg);
            }
        }
    }
    pbuff_free(pbuff);
}

//
//
//

void test_bdd() {
    if ( 0 ) {
        fprintf(stdout,"+ sizeof(rva) = %d\n",(int)sizeof(rva));
        fprintf(stdout,"+ sizeof(rva_node) = %d\n",(int)sizeof(rva_node));
        fprintf(stdout,"+ sizeof(dict_var) = %d\n",(int)sizeof(dict_var));
        fprintf(stdout,"+ sizeof(dict_val) = %d\n",(int)sizeof(dict_val));
        fprintf(stdout,"+ sizeof(bdd_dictionary) = %d\n",(int)sizeof(bdd_dictionary));
    }
#ifdef BDD_VERBOSE
    fprintf(stdout,"# BDD_VERBOSE = ON!\n");
#endif
    if (1) test_regenerate(); // do this always, catches many errors
    if (1) test_trio();       // ,,
    if (1) random_test(1000/*n*/, 888/*seed*/);
    //
    if (1) test_bdd_creation();
    if (0) test_bdd_probability();
    if (0) test_timings();
    if (0) compare_apply_text();
    if (0) test_static_bdd();
    if (0) test_apply();
}
