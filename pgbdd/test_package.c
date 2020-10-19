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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"
#include "bdd.h"
#include "utils.h"
#include "dictionary.h"

/*
 *
 *
 */

DefVectorH(int);
DefVectorC(int);

static int cmpInt(int* l, int* r) { return *l - *r; }

static void test_int(){
    fprintf(stdout,"Test \"V_int\" start\n");
    V_int vv;
    V_int_init(&vv);
    for (int i=0; i<66; i++) {
        V_int_add(&vv,i);
    }
    V_int_delete(&vv,10);
    V_int_delete(&vv,30);
    V_int_set(&vv,5,5555);
    //
    V_int_shrink2size(&vv);
    void* buff = MALLOC(V_int_bytesize(&vv));
    V_int* vc = V_int_serialize(buff,&vv);
    //
    V_int_quicksort(&vv,0,vv.size-1,cmpInt);
    V_int_quicksort(vc,0,vc->size-1,cmpInt);
    for(int i=0; i<V_int_size(vc); i++) {
        // fprintf(stdout,"v[%d]=%d ",i,V_int_get(vc,i));
        if ( V_int_get(vc,i) != V_int_get(&vv,i) )
            vector_error("Should not happen 1");
    }
    // putc('\n', stdout);
    int tofind = 9;
    if ( V_int_find(vc,cmpInt,&tofind) != V_int_bsearch(vc,cmpInt,0,vc->size-1,&tofind) )
        vector_error("Should not happen-2");
    V_int_free(&vv);
    V_int_free(vc);
    FREE(buff);
    fprintf(stdout,"Test \"V_int\" OK\n");
}

/*
 *
 *
 */

typedef struct IV {
    int     i;
    float   v;
} IV;

DefVectorH(IV);
DefVectorC(IV);

static int cmpIVi(IV *l, IV *r) { return l->i - r->i; }
static int cmpIVv(IV *l, IV *r) { return l->v<r->v?-1:(l->v==r->v?0:1); }

static void test_IV(){
    fprintf(stdout,"Test \"V_IV\" start\n");
    V_IV svv;
    V_IV* vv = V_IV_init(&svv);
    for (int i=0; i<33; i++) {
        IV iv;
        iv.i = i;
        iv.v = (float)(i+((i%2==0)?8000:9000));
        V_IV_add(vv,iv);
    }
    //
    void* buff = MALLOC(V_IV_bytesize(vv));
    V_IV* vc = V_IV_serialize(buff,vv);
    //
    //
    int last = -1;
    V_IV_quicksort(vv,0,vv->size-1,cmpIVi);
    for(int i=0; i<vv->size; i++) {
        if ( V_IV_get(vv,i).i < last )
            vector_error("Should not happen, not sorted");
        last = V_IV_get(vv,i).i;
    }
    //
    float lastf = -1;
    V_IV_quicksort(vc,0,vc->size-1,cmpIVv);
    for(int i=0; i<vc->size; i++) {
        if ( V_IV_get(vc,i).v < last )
            vector_error("Should not happen, not sorted");
        lastf = V_IV_get(vv,i).v;
        int fi = V_IV_bsearch(vv,cmpIVi,0,vv->size-1,V_IV_getp(vc,i));
        if ( V_IV_get(vv,fi).v != V_IV_get(vc,i).v )
            vector_error("Should not happen-9 fi=%d [%f <> %f]",fi,vv->items[fi].v,vc->items[i].v);
    }
    //
    V_IV_free(vv);
    V_IV_free(vc);
    FREE(buff);
    fprintf(stdout,"Test \"V_IV\" OK\n");
}

/*
 *
 *
 */

static int test_pbuff(){
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    for(int i=0; i<1000000; i++)
        bprintf(pbuff," %d ",i);
    fprintf(stdout,"+ buffsize=%ld\n",strlen(pbuff->buffer));
    pbuff_reset(pbuff);
    bprintf(pbuff,"This %s a %s ","is","test");
    pbuff_flush(pbuff,stdout);
    bprintf(pbuff,"of the %s pbuffer %s\n","new","class");
    pbuff_flush(pbuff,stdout);
    pbuff_free(pbuff);
    return 1;
}

/*
 *
 *
 */

static void test_vector(){
    test_int();
    test_IV();
}

int main(){
    if (0) test_pbuff();
    if (1) test_vector();
    if (1) test_bdd();
    if (1) test_bdd_dictionary();
}
