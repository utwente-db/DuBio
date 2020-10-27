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
#include "utils.h"
#include "dictionary.h"
#include "bdd.h"

DefVectorC(dict_var);

int cmpDict_var(dict_var* l, dict_var* r) {  return strcmp(l->name,r->name); }

DefVectorC(dict_val);

int bdd_dictionary_is_serialized(bdd_dictionary* dict) {
     return V_dict_var_is_serialized(dict->variables) &&
            V_dict_val_is_serialized(dict->values);
}

bdd_dictionary* bdd_dictionary_serialize(bdd_dictionary* dict) {
    bdd_dictionary* res = NULL;

    if ( 0 /* INCOMPLETE */ && bdd_dictionary_is_serialized(dict) ) {
        // fprintf(stdout,"DICT[%s]: SERIALIZED\n",dict->name);
    } else {
        // fprintf(stdout,"DICT[%s]: NOT SERIALIZED\n",dict->name);
        int varsize = V_dict_var_bytesize(dict->variables);
        int valsize = V_dict_val_bytesize(dict->values);
        int newsize = BDD_DICTIONARY_BASESIZE + varsize + valsize;
        if ( !(res  = (bdd_dictionary*)MALLOC(newsize)) )
            return NULL;
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
    strncpy(dict->name,name,MAX_RVA_NAME);
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

bdd_dictionary* bdd_dictionary_relocate(bdd_dictionary* dict) {
    dict->variables  = (V_dict_var*)(&dict->buff[dict->var_offset]);
    V_dict_var_relocate(dict->variables);
    dict->values     = (V_dict_val*)(&dict->buff[dict->val_offset]);
    V_dict_val_relocate(dict->values);
    if ( !bdd_dictionary_is_serialized(dict) ) 
        vector_error("bdd_dictionary_relocate: dictionary was not serialized");
    return dict;
}

int bdd_dictionary_free(bdd_dictionary* dict) {
    V_dict_var_free(dict->variables);
    V_dict_val_free(dict->values);
    dict->name[0]   = 0;
    dict->variables = NULL;
    dict->values    = NULL;
    return 1;
}

void bdd_dictionary_print(bdd_dictionary* dict, pbuff* pbuff) {
    // bprintf(pbuff,"Dictionary(name=\"%s\")[\n",dict->name);
    if ( 0 ) {
        bprintf(pbuff,"# size=%d\n",dict->size);
        bprintf(pbuff,"# sorted=%d\n",dict->var_sorted);
        bprintf(pbuff,"# val_deleted=%d\n",dict->val_deleted);
        bprintf(pbuff,"# serialized=%d\n",bdd_dictionary_is_serialized(dict));
        bprintf(pbuff,"# magic#=%d\n",dict->magic);
    }
    if ( 0 ) {
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
            for(int j=varp->offset; j<(varp->offset+varp->cardinality); j++) {
                dict_val* valp = V_dict_val_getp(dict->values,j);
                bprintf(pbuff,"%s=%d:%f; ",varp->name,valp->value,valp->prob);
            }
            bprintf(pbuff,"\n");
        }
    }
    // bprintf(pbuff,"]\n");
}

int lookup_var_index(bdd_dictionary* dict, char* name) {
    dict_var tofind;

    strcpy(tofind.name, name);
    if ( dict->var_sorted )
        return V_dict_var_bsearch(dict->variables,cmpDict_var,0,dict->variables->size-1,&tofind);
    else
        return V_dict_var_find(dict->variables,cmpDict_var,&tofind);
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

int bdd_dictionary_sort(bdd_dictionary* dict) {
    V_dict_var_quicksort(dict->variables,0,dict->variables->size-1,cmpDict_var);
    dict->var_sorted = 1;
    return 1;
}

static int normalize_var(bdd_dictionary* dict, dict_var* varp) { 
    double total = 0.0;

    for(int i=varp->offset; i<(varp->offset+varp->cardinality); i++) {
        dict_val* valp = V_dict_val_getp(dict->values,i);
        total += valp->prob;
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
    dict_var newvar = { .offset=V_dict_val_size(dict->values), .cardinality=0 };
    strcpy(newvar.name,name);

    dict->var_sorted = 0;
    int index = V_dict_var_add(dict->variables,&newvar);
    return V_dict_var_getp(dict->variables,index);
}

static dict_val* new_val(bdd_dictionary* dict, int value, double prob) {
    dict_val newval = { .value = value, .prob = prob };

    int index = V_dict_val_add(dict->values,&newval);
    return V_dict_val_getp(dict->values,index);
}


static int get_var_value_index(bdd_dictionary* dict, dict_var* varp, int val) {
    if ( val >= 0 ) {
        for(int i=varp->offset; i<(varp->offset+varp->cardinality); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            if (  val == valp->value )
                return i;
        }
    }
    return -1;
}

static int update_var_val(bdd_dictionary* dict, char* var, char* s_val, char* s_prob, int update, char** errmsg) {
    int var_index, val_index;

    if ( (var_index=lookup_var_index(dict,var)) < 0 )
        return pg_error(errmsg,"bdd_dictionary:update_var_val: no var \"%s\"",var);
    dict_var* varp = V_dict_var_getp(dict->variables,var_index);
    if ( strcmp(s_val,"*")==0 ) {
        // delete entire var
        dict->val_deleted += varp->cardinality;
        V_dict_var_delete(dict->variables,var_index);
        return 1;
    } 
    int i_val = bdd_atoi(s_val);
    if ( (val_index=get_var_value_index(dict,varp,i_val)) < 0)
        return pg_error(errmsg,"bdd_dictionary: %s=%s value not found",var,s_val);
    if ( !update /* delete */ ) {
        dict->val_deleted++;
        if ( --varp->cardinality == 0) 
            return update_var_val(dict,var,"*",NULL,0/*delete*/,errmsg); // delete entire variable
        else {
            for(int i=val_index; i<(varp->offset+varp->cardinality); i++) {
                dict->values->items[i] = dict->values->items[i+1]; // UGLY!!!!
            }
            return 1;
        }
    } else if ( update && s_prob ) {
        double newprob = bdd_atof(s_prob);

        if ( newprob < 0.0 ) 
            return pg_error(errmsg,"bdd_dictionary:update_var_val: %s=%s : %s bad probability",var,s_val,s_prob);
        else {
            dict_val* valp = V_dict_val_getp(dict->values,val_index);
            valp->prob = newprob;
            return 1;
        }
    }
    return 0;
}

double dictionar_lookup_prob(bdd_dictionary* dict,rva* rva) {
    int val_index;

    dict_var* varp = bdd_dictionary_lookup_var(dict,rva->var);
    if ( varp && ((val_index=get_var_value_index(dict,varp,rva->val))>=0) )
        return dict->values->items[val_index].prob;
    return -1.0;
}

int bdd_dictionary_delvars(bdd_dictionary* dict, char* delvars, char** errmsg) { 
    dict_var* varp          = NULL;
    int       dict_varcount = -1;
    char*     p             = delvars;

    do {
        while(isspace(*p)) p++;
        if (*p) {
            char var[MAX_RVA_NAME], val[MAX_RVA_NAME];

            if ( !(
                scantoken(var, &p,'=',MAX_RVA_NAME) &&
                scantoken(val, &p,';',MAX_RVA_NAME) ) )
                return pg_error(errmsg,"bdd_dictionary_delvars: bad syntax: %s",delvars);
            if ( varp && (strcmp(varp->name,var)!=0) ) {
                if (dict->variables->size == dict_varcount )  
                    normalize_var(dict,varp); // only if varp was not deleted
                varp = NULL;
            }
            if ( !varp )
                varp = bdd_dictionary_lookup_var(dict,var);
                dict_varcount = dict->variables->size;
            if ( !update_var_val(dict,var,val,NULL,0/*delete*/,errmsg) )
                return 0;
        }
    } while (p && *p);
    if ( varp )  {
        if (dict->variables->size == dict_varcount )  
            normalize_var(dict,varp); // only if varp was not deleted
}
    return 1;
}

int bdd_dictionary_addvars(bdd_dictionary* dict, char* newvars, int update, char** errmsg) { 
    dict_var* varp = NULL;
    char*     p = newvars;

    do {
        while(isspace(*p)) p++;
        if (*p) {
            char var[MAX_RVA_NAME], s_val[MAX_RVA_NAME], s_prob[MAX_RVA_NAME];

            char* tstart = p;
            if ( !(
                scantoken(var,   &p,'=',MAX_RVA_NAME) &&
                scantoken(s_val, &p,':',MAX_RVA_NAME) &&
                scantoken(s_prob,&p,';',MAX_RVA_NAME) ) )
                return pg_error(errmsg,"bdd_dictionary_add: bad syntax: %s",tstart);
            // fprintf(stdout,"SCAN \"%s\" = \"%s\" : \"%s\"\n",var,val,prob); 
            if ( varp && (strcmp(varp->name,var)!=0) ) {
                normalize_var(dict,varp); // start with a new var
                varp = NULL;
            } 
            if ( !varp )
                if ( !(varp=bdd_dictionary_lookup_var(dict,var)) )
                    varp = new_var(dict,var);
            int    i_val  = bdd_atoi(s_val);
            double d_prob = bdd_atof(s_prob);
            if ( (i_val<0) || (d_prob<0) )
                return pg_error(errmsg,"bdd_dictionary_addvars: bad rva-prob: %s=%s:%s",var,s_val,s_prob);
            if ( get_var_value_index(dict,varp,i_val) >= 0) {
                if ( update ) {
                    if (!update_var_val(dict,var,s_val,s_prob,update,errmsg) )
                        return 0;
                } else
                    return pg_error(errmsg,"bdd_dictionary_add: variable/value %s=%s alreay exists",var,s_val);
            } else {
                if ( !new_val(dict,i_val,d_prob)  )
                    return pg_error(errmsg,"bdd_dictionary_addvars: internal error on: %s=%s:%s",var,s_val,s_prob);
                varp->cardinality++;
            }
        }
    } while (p && *p);
    if ( varp ) 
        normalize_var(dict,varp);
    return 1;
}
