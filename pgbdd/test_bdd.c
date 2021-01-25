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
        bprintf(pbuff,"%s",rand_not(conf));
        bprintf(pbuff,"%s=%d",rand_var(conf),rand_val(conf));
    }
}

static void generate_cluster(randexpr* conf, pbuff* pbuff, int level) {
    int n_expr;
    char* op;
    n_expr = randInRange(1,conf->MAX_CLUSTER_SIZE); 
    op = rand_and_or(conf);
    bprintf(pbuff,"%s",rand_not(conf));
    bprintf(pbuff,"(");
    for (int i=0; i<n_expr; i++) {
        if (i) bprintf(pbuff,"%s",op); 
        generate_expression(conf,pbuff,level);
    }
    bprintf(pbuff,")");
}

static void generate_level(randexpr* conf, pbuff* pbuff, int level) {
    int n_clusters;
    char* op;
    n_clusters = randInRange(1,conf->MAX_CLUSTER); 
    op = rand_and_or(conf);
    bprintf(pbuff,"(");
    for (int i=0; i<n_clusters; i++) {
        if (i) bprintf(pbuff,"%s",op); 
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
 *
 *
 */

static bdd_dictionary* get_test_dictionary(char* dict_vars, char** _errmsg) {
    bdd_dictionary dict_struct, *new_dict;
    bdd_dictionary* res;

    if ( ! (new_dict = bdd_dictionary_create(&dict_struct)) ) {
        pg_error(_errmsg,"get_test_dictionary: error creating dictionary");
        return NULL;
    }
    if ( ! modify_dictionary(new_dict,DICT_ADD,dict_vars,_errmsg))
        return NULL;
    res = dictionary_prepare2store(new_dict);
    return res;
}

static bdd* get_test_bdd(char* expr, int verbose, char** _errmsg) {
    return create_bdd(BDD_DEFAULT,expr,_errmsg,verbose);
}

char *bdd_expr[] = {
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
    char *bdd1_str, *bdd2_str;

    int VERBOSE = 0;

    if (VERBOSE) fprintf(stdout,"TESTING: %s\n",par_expr);
    if ( !(bdd1 = create_bdd(BDD_DEFAULT,par_expr,_errmsg,0)) )
        return 0;
    bdd2string(pbgen,bdd1,0);
    bdd1_str = pbuff_preserve_or_alloc(pbgen);
    if (VERBOSE) fprintf(stdout,"TESTING: bdd1 = [%s]\n",bdd1_str);
    //
    pbgen=pbuff_init(&pbgen_struct);
    if ( !(bdd2 = create_bdd(BDD_DEFAULT,bdd1_str,_errmsg,0)) )
        return 0;
    bdd2string(pbgen,bdd2,0);
    bdd2_str = pbuff_preserve_or_alloc(pbgen);
    if (VERBOSE) fprintf(stdout,"TESTING: bdd2 = [%s]\n",bdd2_str);
    //
    if ( strcmp(bdd1_str,bdd2_str) != 0 ) {
        int res_eq;
        if (0) {
           fprintf(stdout,"expr = [%s]\n",par_expr);
           fprintf(stdout,"bdd1 = [%s]\n",bdd1_str);
           fprintf(stdout,"bdd2 = [%s]\n",bdd2_str);
           fprintf(stdout,"\n");
        }
        if (VERBOSE) fprintf(stdout,"TESTING: NOT STRING EQUAL\n");
        if (VERBOSE) fprintf(stdout,"TESTING: START EQUIV TEST\n");
        res_eq = bdd_test_equivalence(bdd1_str,bdd2_str,_errmsg);
        if (VERBOSE) fprintf(stdout,"TESTING: END EQUIV TEST\n");
        if ( res_eq < 0 )
            pg_fatal("regenerate_test:equivalence error: %s",_errmsg);
        if ( !res_eq ) {
            if (VERBOSE) fprintf(stdout,"TESTING: EQUIVALENT FALSE\n");
            if ( 1 ) {
                fprintf(stdout,"/--------------------------------------------\n");
                fprintf(stdout,"1> %s\n",bdd1_str);
                fprintf(stdout,"/--------------------------------------------\n");
                fprintf(stdout,"2> %s\n",bdd2_str);
                fprintf(stdout,"/--------------------------------------------\n");
            }
            return pg_error(_errmsg,"regenerated string not equivalent to original: %s <<<>>> %s", bdd1_str,bdd2_str);
        } else {
            if (VERBOSE) fprintf(stdout,"TESTING: EQUIVALENT TRUE\n");
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

static void test_create_time(){
    char* _errmsg;
    bdd* pbdd;
    int  TOTAL, count, msec;

    pbdd = NULL;
    TOTAL = 1000000;
    CLOCK_START();
    count  = 0;
    while ( count < TOTAL ) {
        for (int i=0; bdd_expr[i]; i++) {
            if ( !(pbdd = get_test_bdd(bdd_expr[i],0/*verbose*/,&_errmsg)))
                pg_fatal("test_create: error creating bdd: %s",_errmsg);
            FREE(pbdd);
            if ( ++count >= TOTAL )
                break;
        }
    }
    CLOCK_STOP();
    msec = CLOCK_MS();
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
    double total;

    if ( ! (dict = get_test_dictionary(TRIO_DICTIONARY,&_errmsg)))
        pg_fatal("test_trio: error creating dictionary: %s",_errmsg);
    total = 0.0;
    for (int i=0; trio_expr[i]; i++) {
        double prob;
        if ( !(pbdd = get_test_bdd(trio_expr[i],0/*verbose*/,&_errmsg)))
            pg_fatal("test_trio: error creating bdd: %s",_errmsg);
        prob = bdd_probability(dict,pbdd,NULL,0,&_errmsg);
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

#define N_LHS 1
static char *lhs_expr[N_LHS] = {
"x=1"
// "x=1|(!y=3)",
// "(x=2&z=4)",
// "x=3"
};
static bdd* lhs_bdd[N_LHS];

#define N_RHS 2
static char *rhs_expr[N_RHS] = {
"y=1",
"y=1&z=2"
// "y=1",
// "(y=2|x=1|x=2)",
// "(y=3|z=4)"
};
static bdd* rhs_bdd[N_RHS];

static int check_apply_test(char op, bdd* l, bdd* r) {
    pbuff t_pbuff_struct, *t_pbuff=pbuff_init(&t_pbuff_struct);
    pbuff a_pbuff_struct, *a_pbuff=pbuff_init(&a_pbuff_struct);
    char* _errmsg;
    bdd *text_res, *apply_res;

    int VERBOSE = 1;
    if ( VERBOSE ) {
        bprintf(t_pbuff,"check_apply_test:\n");
        bprintf(t_pbuff,"L: ");
        bdd2string(t_pbuff,l,0);
        bprintf(t_pbuff,"\n");
        bprintf(t_pbuff,"R: ");
        bdd2string(t_pbuff,r,0);
        bprintf(t_pbuff,"\n");
        pbuff_flush(t_pbuff,stdout);
    }
    if (!(text_res=bdd_operator(op,BY_TEXT,l,r,&_errmsg)))
        pg_fatal("check_apply_test: error: %s",_errmsg);
    if (!(apply_res=bdd_operator(op,BY_APPLY,l,r,&_errmsg)))
        pg_fatal("check_apply_test: error: %s",_errmsg);
    bdd2string(t_pbuff,text_res,0);
    bdd2string(a_pbuff,text_res,0);
    if ( VERBOSE ) {
        fprintf(stdout,"T: %s\n",t_pbuff->buffer);
        fprintf(stdout,"A: %s\n",a_pbuff->buffer);
    }
    if ( strcmp(t_pbuff->buffer,t_pbuff->buffer) != 0 ) {
        // NOT EQUAL
        int eqv = bdd_test_equivalence(t_pbuff->buffer,t_pbuff->buffer,&_errmsg);
        if ( eqv == BDD_FAIL )
            pg_fatal("check_apply_test: error: %s",_errmsg);
        if ( ! eqv ) 
            pg_fatal("check_apply_test: NOT EQUAL");
    }
    //
    pbuff_free(t_pbuff); pbuff_free(a_pbuff);
    return 1;
}

static int run_check_apply() {
    char* _errmsg;

    for (int i=0; i<N_LHS; i++) 
        if (!(lhs_bdd[i] = create_bdd(BDD_DEFAULT,lhs_expr[i],&_errmsg,0)))
            pg_fatal("compare_apply_text: error: %s",_errmsg);
    for (int i=0; i<N_RHS; i++) 
        if (!(rhs_bdd[i] = create_bdd(BDD_DEFAULT,rhs_expr[i],&_errmsg,0)))
            pg_fatal("compare_apply_text: error: %s",_errmsg);
    //
    for(int i=0; i<N_LHS; i++) {
        for(int j=0; j<N_RHS; j++) {
            if ( ! check_apply_test('&', lhs_bdd[i],rhs_bdd[j]) )
                return 0;
        }
    }
    return 1;
} 


static void run_apply_test(op_mode m) {
    char* _errmsg = NULL;

    int repeat = 100000;
    int count = 0;
    int msec;
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
    msec = CLOCK_MS();
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
    bdd *lhs, *rhs,*res;
    // char* dotfile = NULL;
    char* dotfile = "./DOT/test.dot";
  
    char* la = "(x=1)";
    //char* la = "(x=1&y=2)";
    char  op = '&';
    // char* ra = "y=2";
    char* ra = "(y=1|y=2)&x=2";

    lhs = create_bdd(BDD_DEFAULT,la,&_errmsg,0/*verbose*/);
    rhs = create_bdd(BDD_DEFAULT,ra,&_errmsg,0/*verbose*/);
    if ( !(lhs && rhs) )
        pg_fatal("test_apply: error: %s",_errmsg);
    res = bdd_apply(op,lhs,rhs,1/*verbose*/,&_errmsg);
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
    // nice example from random generator
    // char* expr = "((c=3&a=4&c=2)|!(b=0|c=2)|(c=3))";

    // char* expr = "((x=1&y=1)|(x=2&y=2))&(z=1&y=2)"; // interesting cutout
    // char* expr = "((c=1&d=1)|(d=2)|!(!c=4|a=4|d=0)|(a=3&c=4&c=2))"; // DIFFICULT!
    // char* expr = "(  x=1 |((y=1) ) )";
    char* expr = "(b=0&(!c=2&c=3)|!c=2)"; // look what happens

    bdd*  test_bdd;
    char* _errmsg = NULL;

    int verbose = 1;
    if ( (test_bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,verbose)) ) {
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
                // exit(0);
                pg_fatal("regenerate_test:error: %s",_errmsg);
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
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char** extra = NULL;
    double prob;

    if ( !dict || !pbdd )
        return -1.0;
    if ( verbose ) {
        bdd_info(pbdd,pbuff); pbuff_flush(pbuff,stdout);
    }
    if ( dotfile && show_prob_in_dot ) {
        char* extra_str_base;
        int n = V_rva_node_size(&pbdd->tree);
        int sz_ptrs = n * sizeof(char*);
        extra = (char**)MALLOC(sz_ptrs + (n * EXTRA_PRINT_SIZE));
        extra_str_base = ((char*)extra)+sz_ptrs;
        for(int i=0; i<n; i++) {
            extra[i]    = extra_str_base + i*EXTRA_PRINT_SIZE;
            *(extra[i]) = 0;
        }
    }
    prob = bdd_probability(dict,pbdd,extra,verbose,_errmsg);
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
 *
 */

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

static void random_test(int n, long seed, int verbose) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
  
    srand(seed);
    for (int i=0; i<n; i++) {
        char* _errmsg = NULL;
        bdd*      bdd = NULL;
        char *expr    = random_expression(&RANDEXPR,pbuff);

        if (verbose) fprintf(stdout,"random> %s\n",expr);
        if (!(bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,0))) {
            pg_fatal("random_test: error: %s",_errmsg);
        }
        FREE(bdd);
        if ( !_regenerate_test(expr,&_errmsg) ) {
            pg_fatal("regenerate_test:error: %s",_errmsg);
        }
    }
    pbuff_free(pbuff);
}



#define EQV_HUNT_LHS_SIZE 10000
#define EQV_HUNT_RHS_SIZE 10000

static void expr_bdd2string(pbuff* pbuff, char* expr) {
    char* _errmsg = NULL;

    bdd* bdd = create_bdd(BDD_DEFAULT,expr,&_errmsg,0);
    if ( !bdd )
        pg_fatal("expr_bdd2string: %s",_errmsg);
    bdd2string(pbuff,bdd,0);
    FREE(bdd);
}

static void random_equiv_hunt(long seed) {
    pbuff pbuff_struct_1, *pb1=pbuff_init(&pbuff_struct_1);
    pbuff pbuff_struct_2, *pb2=pbuff_init(&pbuff_struct_2);
    char *_errmsg = NULL;
    char *lhs[EQV_HUNT_LHS_SIZE];
  
    srand(seed);
    for (int l_i=0; l_i<EQV_HUNT_LHS_SIZE; l_i++) {
        lhs[l_i] = strdup(random_expression(&RANDEXPR,pb1));
    }
    for (int r_i=0; r_i<EQV_HUNT_RHS_SIZE; r_i++) {
        char *r_expr    = random_expression(&RANDEXPR,pb1);
        for (int l_i=0; l_i<EQV_HUNT_RHS_SIZE; l_i++) {
            int eqv = bdd_test_equivalence(lhs[l_i],r_expr,&_errmsg);
            if ( eqv < 0 )
                pg_fatal("random_equiv_hunt: error during equiv hunt");
            if ( eqv ) {
                char *l;

                pbuff_flush(pb2,NULL);
                expr_bdd2string(pb2,lhs[l_i]);
                //
                if (strlen(pb2->buffer) == 1 )
                    continue;
                //
                l = strdup(pb2->buffer);
                pbuff_flush(pb2,NULL);
                expr_bdd2string(pb2,r_expr);
                //
                if ( strcmp(l,pb2->buffer) == 0 ) {
                    FREE(l);
                    continue;
                }
                fprintf(stdout,"Found equivalence:\n");
                fprintf(stdout,"LG: %s\n",l);
                fprintf(stdout,"LG: %s\n",pb2->buffer);
                FREE(l);
            }
        }
    }
    for (int l_i=0; l_i<EQV_HUNT_LHS_SIZE; l_i++) {
        FREE(lhs[l_i]);
    }
    pbuff_free(pb1);pbuff_free(pb2);
}

//
//
//

#define CHECK0(L,STAT) if ( !(STAT) )    pg_fatal("%s: %s",L,_errmsg)
#define CHECKN(L,STAT) if ( (STAT) < 0 ) pg_fatal("%s: %s",L,_errmsg)

static char* _wb_dot(char* base, char* extra) {
    static char fnbuff[32];
    sprintf(fnbuff,"./DOT/%s%s.dot",base,extra);
    return fnbuff;
}

static int _wb_equiv(bdd* l,bdd* r,int vb,char**_errmsg)  {
    pbuff pbuff_struct_l, *pbuff_l=pbuff_init(&pbuff_struct_l);
    pbuff pbuff_struct_r, *pbuff_r=pbuff_init(&pbuff_struct_r);
    int res;

    bdd2string(pbuff_l,l,0);
    bdd2string(pbuff_r,r,0);
    if (vb) fprintf(stdout,"EQV-L=%s:\n",pbuff_l->buffer);
    if (vb) fprintf(stdout,"EQV-R=%s:\n",pbuff_r->buffer);
    CHECKN("_wb_equiv",res = bdd_test_equivalence(pbuff_l->buffer,pbuff_r->buffer,_errmsg));
    if (vb) fprintf(stdout,"EQV-RES=%d:\n",res);
    return res;
}

static bdd* _wb_op(bdd* bdd_l,char op, bdd* bdd_r,int vb,char**_errmsg)  {
    bdd* bdd_res_t;
    bdd* bdd_res_a;

    CHECK0("_wb_op",bdd_res_t=bdd_operator(op,BY_TEXT,bdd_l,bdd_r,_errmsg));
    CHECK0("_wb_op",bdd_res_a=bdd_operator(op,BY_APPLY,bdd_l,bdd_r,_errmsg));

    return bdd_res_a;
}

static int _wb_regen(char* label, bdd* par_bdd, int vb, int dot, char**_errmsg)  {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char* bdd_str;
    bdd* bdd_regen;

    bdd2string(pbuff,par_bdd,0);
    bdd_str = pbuff_preserve_or_alloc(pbuff);
    CHECK0("_wb_regen",bdd_regen=create_bdd(BDD_DEFAULT,bdd_str,_errmsg,0));
    if (dot) bdd_generate_dotfile(bdd_regen,_wb_dot(label,"_regen"),NULL);
    _wb_equiv(par_bdd,bdd_regen,vb,_errmsg);
    pbuff_free(pbuff);
    //
    return 1;
}

static void _wb(char* l, char op, char *r, int rg, int vb, int dot)  {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char* _errmsg;
    bdd *bdd_l=0, *bdd_r=0, *bdd_res=0;

    switch ( op ) {
        case 0:
            CHECK0("_wb",bdd_l=create_bdd(BDD_DEFAULT,l,&_errmsg,vb));
            bdd2string(pbuff,bdd_l,0);
            fprintf(stdout,"ORIGINAL  : %s\n",l);
            fprintf(stdout,"REGENERATE: %s\n",pbuff->buffer);
            break;
        case '!':
            CHECK0("_wb",bdd_l=create_bdd(BDD_DEFAULT,l,&_errmsg,vb));
            CHECK0("_wb",bdd_res=_wb_op(bdd_l,op,NULL,vb,&_errmsg));
            break;
        case '&':
        case '|':
            CHECK0("_wb",bdd_l=create_bdd(BDD_DEFAULT,l,&_errmsg,vb));
            CHECK0("_wb",bdd_r=create_bdd(BDD_DEFAULT,r,&_errmsg,vb));
            CHECK0("_wb",bdd_res=_wb_op(bdd_l,op,bdd_r,vb,&_errmsg));
            break;
        case '=':
            CHECK0("_wb",bdd_l=create_bdd(BDD_DEFAULT,l,&_errmsg,vb));
            CHECK0("_wb",bdd_r=create_bdd(BDD_DEFAULT,r,&_errmsg,vb));
            _wb_equiv(bdd_l,bdd_r,vb,&_errmsg);
    }
    //
    if ( rg ) {
        if (bdd_l)   _wb_regen("bdd_l",bdd_l,vb,dot,&_errmsg);
        if (bdd_r)   _wb_regen("bdd_r",bdd_r,vb,dot,&_errmsg);
        if (bdd_res) _wb_regen("bdd_res",bdd_res,vb,dot,&_errmsg);
    }
    if ( dot ) {
        if ( bdd_l)   bdd_generate_dotfile(bdd_l,_wb_dot("bdd_l",""),NULL);
        if ( bdd_r)   bdd_generate_dotfile(bdd_r,_wb_dot("bdd_r",""),NULL);
        if ( bdd_res) bdd_generate_dotfile(bdd_res,_wb_dot("bdd_res",""),NULL);
    }
    //
    pbuff_free(pbuff);
}

static void workbench() {
    _wb("x=1",'=',"(x=1)&!!(x=1)",0/*rg*/,1/*vb*/,1/*dot*/);
    // _wb("x=1|(y=2&x=4)|(z=3&x=4)",0,0,0/*rg*/,1/*vb*/,1/*dot*/);
    // _wb("x=87943579265023650826531",0,0,0/*rg*/,0/*vb*/,1/*dot*/);
    // _wb("x=1",'&',"x=2",0/*rg*/,1/*vb*/,1/*dot*/);
}


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
    if (1) test_regenerate(); // do these 3 tests always, catches many errors
    if (1) random_test(1000/*n*/, 888/*seed*/, 0/*verbose*/);
    if (1) test_trio();       
    //
    if (0) random_equiv_hunt(999);
    if (0) test_bdd_creation();
    if (0) test_bdd_probability();
    if (0) test_create_time();
    if (0) compare_apply_text();
    if (0) test_static_bdd();
    if (0) test_apply();
    if (0) run_check_apply();
    if (1) workbench();
}
