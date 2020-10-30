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


#include "utils.h"
#include "vector.h"

DefVectorH(int);
DefVectorC(int);

static int cmpInt(int* l, int* r) { return *l - *r; }

static void test_int(){
    fprintf(stdout,"Test \"V_int\" start\n");
    V_int vv;
    V_int_init(&vv);
    for (int i=0; i<66; i++) {
        V_int_add(&vv,&i);
    }
    V_int_delete(&vv,10);
    V_int_delete(&vv,30);
    V_int_set(&vv,5,5555);
    //
    V_int_shrink2size(&vv);
    void* buff = MALLOC(V_int_bytesize(&vv));
    V_int* vc = V_int_serialize(buff,&vv);
    //
    V_int_quicksort(&vv,cmpInt);
    V_int_quicksort(vc,cmpInt);
    for(int i=0; i<V_int_size(vc); i++) {
        // fprintf(stdout,"v[%d]=%d ",i,V_int_get(vc,i));
        if ( V_int_get(vc,i) != V_int_get(&vv,i) )
            pg_fatal("Should not happen 1");
    }
    // putc('\n', stdout);
    int tofind = 9;
    if ( V_int_find(vc,cmpInt,&tofind) != V_int_bsearch(vc,cmpInt,&tofind) )
        pg_fatal("Should not happen-2");
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

static void print_IV(V_IV* v_iv) {
    fprintf(stdout,"V_IV[0-%d]:\n",V_IV_size(v_iv));
    for (int i=0; i<V_IV_size(v_iv); i++ ) {
        IV* ivp = V_IV_getp(v_iv,i);
        fprintf(stdout,"[%2d]: <%d, %f>\n",i,ivp->i,ivp->v);
    }
}

static void test_IV(){
    fprintf(stdout,"Test \"V_IV\" start\n");
    V_IV svv;
    V_IV* vv = V_IV_init(&svv);
    for (int i=0; i<20; i++) {
        IV iv;
        iv.i = i;
        iv.v = (float)(i+((i%2==0)?8000:9000));
        V_IV_add(vv,&iv);
    }
    //
    print_IV(&svv);
    // V_IV_copy_range(&svv,5,10,10);
    V_IV_delete(&svv,5);
    print_IV(&svv);
    IV iiv = { .i=55, .v=5500 };
    V_IV_insert_at(&svv,5,&iiv);
    print_IV(&svv);
    exit(0);
    //
    void* buff = MALLOC(V_IV_bytesize(vv));
    V_IV* vc = V_IV_serialize(buff,vv);
    //
    //
    int last = -1;
    V_IV_quicksort(vv,cmpIVi);
    for(int i=0; i<vv->size; i++) {
        if ( V_IV_get(vv,i).i < last )
            pg_fatal("Should not happen, not sorted");
        last = V_IV_get(vv,i).i;
    }
    //
    float lastf = -1;
    V_IV_quicksort(vc,cmpIVv);
    for(int i=0; i<vc->size; i++) {
        if ( V_IV_get(vc,i).v < last )
            pg_fatal("Should not happen, not sorted");
        lastf = V_IV_get(vv,i).v;
        int fi = V_IV_bsearch(vv,cmpIVi,V_IV_getp(vc,i));
        if ( V_IV_get(vv,fi).v != V_IV_get(vc,i).v )
            pg_fatal("Should not happen-9 fi=%d [%f <> %f]",fi,vv->items[fi].v,vc->items[i].v);
    }
    //
    V_IV_free(vv);
    V_IV_free(vc);
    FREE(buff);
    fprintf(stdout,"Test \"V_IV\" OK\n");
}

//
//
//

void test_vector(){
    if (0) test_int();
    if (1) test_IV();
}
