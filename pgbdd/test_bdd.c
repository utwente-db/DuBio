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

void test_bdd(){
    char* _errmsg = NULL;

    // char* expr = "((x=1 and x=1) or (not y=0 and not zzz=1)) and((xx=2 and x=3)or(not x=2 and not x=3))";
    // char* expr = "((x=1 | x=2) & x=2)";
    // char* expr = "x=1 & x=2 | x=3 & x=4 | x=5 & x=6 | x=7 & x=8";
    // (x0 and y1) or (not x0 and not y1) or ((x2 and y2) and y34)
    char* expr = "(x=1 | x=2)";
    bdd* test_bdd;

    if ( (test_bdd = create_bdd(expr,&_errmsg,1/*verbose*/)) ) {
        char* dot_filename = "./DOT/test.dot";

        fprintf(stdout,"Created bdd: %s\n",test_bdd->expr);
        bdd_print_tree(&test_bdd->tree,stdout);
        if ( dot_filename ) {
            FILE* f = fopen(dot_filename,"w");

            if ( ! f )
                fprintf(stderr,"Warning: unable to write dotfile: %s\n",dot_filename);
            pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
            bdd_generate_dot(test_bdd,pbuff);
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
