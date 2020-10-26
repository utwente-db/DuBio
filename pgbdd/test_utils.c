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

void test_utils() {
    if ( 0 ) test_pbuff();
    if ( 1 ) run_bee_testset();
}
