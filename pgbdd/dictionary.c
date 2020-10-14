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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vector.h"
#include "bdd.h"
#include "utils.h"
#include "dictionary.h"

DefVectorC(dict_var);

int cmpDict_var(dict_var l, dict_var r) {  return strcmp(l.name,r.name); }

DefVectorC(dict_val);

static int bdd_dictionary_is_serialized(bdd_dictionary* dict) {
     return V_dict_var_is_serialized(dict->variables) &&
            V_dict_val_is_serialized(dict->values);
}

bdd_dictionary* bdd_dictionary_serialize(bdd_dictionary* dict) {
    bdd_dictionary* res = NULL;

    if ( 0 /* INCOMPLETE */ && bdd_dictionary_is_serialized(dict) ) {
        fprintf(stdout,"DICT[%s]: SERIALIZED\n",dict->name);
    } else {
        fprintf(stdout,"DICT[%s]: NOT SERIALIZED\n",dict->name);
        int varsize = V_dict_var_bytesize(dict->variables);
        int valsize = V_dict_val_bytesize(dict->values);
        int newsize = BDD_DICTIONARY_BASESIZE + varsize + valsize;
        res  = (bdd_dictionary*)MALLOC(newsize);
        *res = *dict;
        res->size = newsize;
        res->var_offset = 0;
        res->variables  = V_dict_var_serialize((void*)(&res->buff[res->var_offset]),dict->variables);
        res->val_offset = varsize;
        res->values     = V_dict_val_serialize((void*)(&res->buff[res->val_offset]),dict->values);
    }
    return res;
}

bdd_dictionary* bdd_dictionary_create(bdd_dictionary* dict, char* name) {
    strncpy(dict->name,name,MAXRVA);
    // incomplete, randomize for time ???
    dict->magic      = rand();
    dict->size       = sizeof(bdd_dictionary);
    dict->var_sorted = 0;
    dict->var_offset = 0;
    dict->val_deleted= 0;
    dict->variables  = V_dict_var_init((V_dict_var*)(&dict->buff[dict->var_offset]));
    dict->val_offset = sizeof(V_dict_var);
    dict->values     = V_dict_val_init((V_dict_val*)(&dict->buff[dict->val_offset]));
    return dict;
}

static void bdd_dictionary_relocate(bdd_dictionary* dict) {
    dict->variables  = (V_dict_var*)(&dict->buff[dict->var_offset]);
    V_dict_var_relocate(dict->variables);
    dict->values     = (V_dict_val*)(&dict->buff[dict->val_offset]);
    V_dict_val_relocate(dict->values);
    if ( !bdd_dictionary_is_serialized(dict) ) 
        vector_error("bdd_dictionary_relocate: dictionary was not serialized");
}

void bdd_dictionary_free(bdd_dictionary* dict) {
    V_dict_var_free(dict->variables);
    V_dict_val_free(dict->values);
    dict->name[0]   = 0;
    dict->variables = NULL;
    dict->values    = NULL;
}

static void bdd_dictionary_print(bdd_dictionary* dict, pbuff* pbuff) {
    bprintf(pbuff,"Dictionary(name=\"%s\"):\n",dict->name);
    if ( 1 ) {
        bprintf(pbuff,"+ size=%d\n",dict->size);
        bprintf(pbuff,"+ sorted=%d\n",dict->var_sorted);
        bprintf(pbuff,"+ val_deleted=%d\n",dict->val_deleted);
        bprintf(pbuff,"+ serialized=%d\n",bdd_dictionary_is_serialized(dict));
        bprintf(pbuff,"+ magic#=%d\n",dict->magic);
    }
    if ( 0 ) {
        bprintf(pbuff,"+ Variables:\n");
        for(int i=0; i<V_dict_var_size(dict->variables); i++) {
            dict_var* varp = V_dict_var_getp(dict->variables,i);
            bprintf(pbuff,"(%d)\t\"%s\"\t o(%d)\t c(%d)\n",i,varp->name,varp->offset,varp->cardinality);
        }
        bprintf(pbuff,"+ Values:\n");
        for(int i=0; i<V_dict_val_size(dict->values); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            bprintf(pbuff,"(%d)\t\"%d\"\t: %f\n",i,valp->value,valp->prob);
        }
    }
    if ( 1 ) {
        for(int i=0; i<V_dict_var_size(dict->variables); i++) {
            dict_var* varp = V_dict_var_getp(dict->variables,i);
            bprintf(pbuff,"{");
            for(int j=varp->offset; j<(varp->offset+varp->cardinality); j++) {
                dict_val* valp = V_dict_val_getp(dict->values,j);
                bprintf(pbuff,"%s=%d:%f; ",varp->name,valp->value,valp->prob);
            }
            bprintf(pbuff,"}\n");
        }
    }
}

static int lookup_var_index(bdd_dictionary* dict, char* name) {
    dict_var tofind;

    strcpy(tofind.name, name);
    if ( dict->var_sorted )
        return V_dict_var_bsearch(dict->variables,cmpDict_var,0,dict->variables->size-1,tofind);
    else
        return V_dict_var_find(dict->variables,cmpDict_var,tofind);
}

dict_var* bdd_dictionary_lookup_var(bdd_dictionary* dict, char* name) {
    int index = lookup_var_index(dict,name);
    return (index < 0 ) ? NULL : V_dict_var_getp(dict->variables,index);
}

static int scantoken(char* to, char** base, char delimiter, int max) {
    int sz = 0;
    char c, *p=*base;

    while ( (c=*p) ) {
        p++;
        if ( !isspace(c) ) {
            if ( c == delimiter ) {
                *to = 0;
                *base = p;
                return 1;
            } else {
                if ( ++sz > max ) // buffer overflow
                    return 0;
                *to++ = c;
            }
        }
    }
    return 0;
}

static void bdd_dictionary_sort(bdd_dictionary* dict) {
    V_dict_var_quicksort(dict->variables,0,dict->variables->size-1,cmpDict_var);
    dict->var_sorted = 1;
}

static int normalize_var(bdd_dictionary* dict, dict_var* varp) { 
    double total = 0.0;

    for(int i=varp->offset; i<(varp->offset+varp->cardinality); i++) {
        dict_val* valp = V_dict_val_getp(dict->values,i);
        total += valp->prob;
        // fprintf(stdout,"FFFF(%d)\t\"%d\"\t: %f\n",i,valp->value,valp->prob);
    }
    if ( total<(1.0-0.0001) || total>(1.0+0.0001) ) {
        double factor = 1.0/total;
        for(int i=varp->offset; i<(varp->offset+varp->cardinality); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            valp->prob *= factor;
        }
    }
    return 1;
}

static dict_var* new_var(bdd_dictionary* dict, char* name) {
    dict_var newvar;

    strcpy(newvar.name,name);
    newvar.offset      = V_dict_val_size(dict->values);
    newvar.cardinality = 0;
    dict->var_sorted = 0;
    int index = V_dict_var_add(dict->variables,newvar);
    return V_dict_var_getp(dict->variables,index);
}

static dict_val* new_val(bdd_dictionary* dict, char* value, char* prob) {
    dict_val newval;

    newval.value = bdd_atoi(value);
    newval.prob  = bdd_atof(prob);
    if ( newval.value < 0 || newval.prob < 0 )
        return NULL;
    else {
        int index = V_dict_val_add(dict->values,newval);
        return V_dict_val_getp(dict->values,index);
    }
}


static int get_var_value_index(bdd_dictionary* dict, dict_var* varp, char* value) {
    int val = bdd_atoi(value);

    if ( val >= 0 ) {
        for(int i=varp->offset; i<(varp->offset+varp->cardinality); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            if (  val == valp->value )
                return i;
        }
    }
    return -1;
}

static int update_var_val(bdd_dictionary* dict, char* s_var, char* s_val, char* s_prob) {
    int var_index, val_index;

    if ( (var_index=lookup_var_index(dict,s_var)) < 0 ) {
        vector_error("bdd_dictionary:update_var_val: no var \"%s\"",s_var);
        return 0;
    }
    dict_var* varp = V_dict_var_getp(dict->variables,var_index);
    if ( strcmp(s_val,"*")==0 ) {
        // delete entire var
        dict->val_deleted += varp->cardinality;
        V_dict_var_delete(dict->variables,var_index);
        return 1;
    } 
    if ( (val_index=get_var_value_index(dict,varp,s_val)) < 0)
        return 0; // value not found
    if ( !s_prob ) {
        // delete var-value alternative
        dict->val_deleted++;
        if ( --varp->cardinality == 0) 
            return update_var_val(dict,s_var,"*",NULL); // delete entire variable
        else {
            for(int i=val_index; i<(varp->offset+varp->cardinality); i++) {
                dict->values->items[i] = dict->values->items[i+1]; // UGLY!!!!
            }
            return normalize_var(dict,varp);
        }
    } else if ( s_prob ) {
        double newprob = bdd_atof(s_prob);

        if ( newprob >= 0.0 ) {
            dict_val* valp = V_dict_val_getp(dict->values,val_index);
            valp->prob = newprob;
            return normalize_var(dict,varp);
        }
    }
    return 0;
}

static int bdd_dictionary_delvars(bdd_dictionary* dict, char* delvars) { 
    char*     p = delvars;

    do {
        while(isspace(*p)) p++;
        if (*p) {
            char var[MAXRVA], val[MAXRVA];

            if ( !(
                scantoken(var, &p,'=',MAXRVA) &&
                scantoken(val, &p,';',MAXRVA) ) ) {
                vector_error("bdd_dictionary_delvars: bad syntax: %s",delvars);
                return 0;
            }
            fprintf(stdout,"DELVARS-SCAN \"%s\" = \"%s\"\n",var,val); 
            if (!update_var_val(dict,var,val,NULL) )
                return 0;
        }
    } while (p && *p);
    return 1;
}

int bdd_dictionary_addvars(bdd_dictionary* dict, char* newvars) { 
    dict_var* varp = NULL;
    char*     p = newvars;

    do {
        while(isspace(*p)) p++;
        if (*p) {
            char var[MAXRVA], val[MAXRVA], prob[MAXRVA];

            if ( !(
                scantoken(var, &p,'=',MAXRVA) &&
                scantoken(val, &p,':',MAXRVA) &&
                scantoken(prob,&p,';',MAXRVA) ) ) {
                vector_error("bdd_dictionary_add: bad syntax: %s",newvars);
                return 0;
            }
            // fprintf(stdout,"SCAN \"%s\" = \"%s\" : \"%s\"\n",var,val,prob); 
            if ( varp && (strcmp(varp->name,var)!=0) ) {
                normalize_var(dict,varp);
                varp = NULL;
            } 
            if ( !varp )
                if ( !(varp=bdd_dictionary_lookup_var(dict,var)) )
                    varp = new_var(dict,var);
            if ( get_var_value_index(dict,varp,val) >= 0) {
                // value already exists, update the value
                if (!update_var_val(dict,var,val,prob) )
                    vector_error("bdd_dictionary_addvars: bad value: %s=%s:%s",var,val,prob);
                    return 0; // error
            } else {
                if ( !new_val(dict,val,prob)  ) {
                    vector_error("bdd_dictionary_addvars: bad value: %s=%s:%s",var,val,prob);
                    return 0;
                }
            }
            varp->cardinality++;
        }
    } while (p && *p);
    if ( varp ) 
        normalize_var(dict,varp);
    return 1;
}

//
//
//

#ifdef TEST_CONFIG

static void test_bdd_dictionary_v0() {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_dictionary dict_struct, *dict;

    char* input = "x=0 : 0.6; x=1 : 0.2; x=2 : 0.1; x=3 : 0.1; y=3 : 0.5; y=1 : 0.2; y=2 : 0.3; q=8 : 1.0; ";
    dict = bdd_dictionary_create(&dict_struct,"XYZ");
    if (bdd_dictionary_addvars(dict,input)) {
        bdd_dictionary_print(dict, pbuff);
        pbuff_flush(pbuff,stdout);
        bdd_dictionary* new_dict = bdd_dictionary_serialize(dict);
        bdd_dictionary_free(dict);
        bdd_dictionary_sort(new_dict);
        bdd_dictionary_lookup_var(new_dict,"x");
        bdd_dictionary_print(new_dict, pbuff);
        pbuff_flush(pbuff,stdout);
        bdd_dictionary* relocate_dict = (bdd_dictionary*)MALLOC(new_dict->size);
        memcpy(relocate_dict,new_dict,new_dict->size);
        bdd_dictionary_relocate(relocate_dict);
        bdd_dictionary_free(new_dict);
        FREE(new_dict);
        bdd_dictionary_print(relocate_dict, pbuff);
        pbuff_flush(pbuff,stdout);
        bdd_dictionary_free(relocate_dict);
        FREE(relocate_dict);
    } else {
        fprintf(stdout,"DICTIONARY ADD VAR failed\n");
    }
}

static void test_bdd_dictionary_v1() {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_dictionary dict_struct, *dict;

    char* input = "x=0 : 0.6; x=1 : 0.2; x=2 : 0.1; x=3 : 0.1; y=3 : 0.2; y=1 : 0.2; y=2 : 0.1; q=8 : 0.5; p=5:0.5; p=6:0.5;";
    dict = bdd_dictionary_create(&dict_struct,"XYZ");
    if (bdd_dictionary_addvars(dict,input)) {
        bdd_dictionary_print(dict, pbuff);
        pbuff_flush(pbuff,stdout);
        //
        bdd_dictionary_delvars(dict,"y=*;");
        bdd_dictionary_print(dict, pbuff);
        pbuff_flush(pbuff,stdout);
        bdd_dictionary_delvars(dict,"x=2;");
        bdd_dictionary_print(dict, pbuff);
        pbuff_flush(pbuff,stdout);
        bdd_dictionary_delvars(dict,"q=8;");
        bdd_dictionary_print(dict, pbuff);
        pbuff_flush(pbuff,stdout);
        bdd_dictionary_addvars(dict,"p=6:1.0;");
        bdd_dictionary_print(dict, pbuff);
        pbuff_flush(pbuff,stdout);
        //
        bdd_dictionary_free(dict);
    } else {
        fprintf(stdout,"DICTIONARY ADD VAR failed\n");
    }
}

void test_bdd_dictionary() {
    if (0) test_bdd_dictionary_v0();
    if (1) test_bdd_dictionary_v1();
}

#endif
