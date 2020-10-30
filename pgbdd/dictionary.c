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

#include "utils.h"
#include "vector.h"
#include "dictionary.h"
#include "bdd.h"

DefVectorC(dict_var);

int cmpDict_var(dict_var* l, dict_var* r) {  return strcmp(l->name,r->name); }

DefVectorC(dict_val);

static int bdd_dictionary_is_serialized(bdd_dictionary* dict) {
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
    if ( (dict->variables == NULL) || (dict->values == NULL) )
        return NULL;
    else
        return dict;
}

bdd_dictionary* bdd_dictionary_relocate(bdd_dictionary* dict) {
    dict->variables  = (V_dict_var*)(&dict->buff[dict->var_offset]);
    V_dict_var_relocate(dict->variables);
    dict->values     = (V_dict_val*)(&dict->buff[dict->val_offset]);
    V_dict_val_relocate(dict->values);
    if ( !bdd_dictionary_is_serialized(dict) ) 
        pg_fatal("bdd_dictionary_relocate: dictionary was not serialized");
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

void bdd_dictionary_print(bdd_dictionary* dict, int all, pbuff* pbuff) {
    // bprintf(pbuff,"Dictionary(name=\"%s\")[\n",dict->name);
    if ( all ) {
        bprintf(pbuff,"# size=%d\n",dict->size);
        bprintf(pbuff,"# sorted=%d\n",dict->var_sorted);
        bprintf(pbuff,"# val_deleted=%d\n",dict->val_deleted);
        bprintf(pbuff,"# serialized=%d\n",bdd_dictionary_is_serialized(dict));
        bprintf(pbuff,"# magic#=%d\n",dict->magic);
    }
    if ( all ) {
        for(int i=0; i<V_dict_var_size(dict->variables); i++) {
            dict_var* varp = V_dict_var_getp(dict->variables,i);
            bprintf(pbuff,"(%d)\t\"%s\"\t o(%d)\t c(%d)\n",i,varp->name,varp->offset,varp->card);
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
            for(int j=varp->offset; j<(varp->offset+varp->card); j++) {
                dict_val* valp = V_dict_val_getp(dict->values,j);
                bprintf(pbuff,"%s=%d:%f; ",varp->name,valp->value,valp->prob);
            }
            bprintf(pbuff,"\n");
        }
    }
    // bprintf(pbuff,"]\n");
}

static int lookup_var_index(bdd_dictionary* dict, char* name) {
    dict_var tofind;

    strcpy(tofind.name, name);
    if ( dict->var_sorted )
        return V_dict_var_bsearch(dict->variables,cmpDict_var,&tofind);
    else
        return V_dict_var_find(dict->variables,cmpDict_var,&tofind);
}

static dict_var* bdd_dictionary_lookup_var(bdd_dictionary* dict, char* name) {
    int index = lookup_var_index(dict,name);
    return (index < 0 ) ? NULL : V_dict_var_getp(dict->variables,index);
}

int bdd_dictionary_sort(bdd_dictionary* dict) {
    V_dict_var_quicksort(dict->variables,cmpDict_var);
    dict->var_sorted = 1;
    return 1;
}

static dict_var* new_var(bdd_dictionary* dict, char* name) {
    dict_var newvar = { .offset=V_dict_val_size(dict->values), .card=0 };
    strcpy(newvar.name,name);

    dict->var_sorted = 0;
    int index = V_dict_var_add(dict->variables,&newvar);
    return (index<0) ? NULL : V_dict_var_getp(dict->variables,index);
}

static dict_val* new_val(bdd_dictionary* dict, int value, double prob) {
    dict_val newval = { .value = value, .prob = prob };

    int index = V_dict_val_add(dict->values,&newval);
    return (index<0) ? NULL : V_dict_val_getp(dict->values,index);
}


static int get_var_value_index(bdd_dictionary* dict, dict_var* varp, int val) {
    if ( val >= 0 ) {
        for(int i=varp->offset; i<(varp->offset+varp->card); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            if (  val == valp->value )
                return i;
        }
    }
    return -1;
}

double lookup_probability(bdd_dictionary* dict,rva* rva) {
    int val_index;

    dict_var* varp = bdd_dictionary_lookup_var(dict,rva->var);
    if ( varp && ((val_index=get_var_value_index(dict,varp,rva->val))>=0) )
        return dict->values->items[val_index].prob;
    return -1.0;
}

/*
 * Dictionary modification functions
 *
 */

#define VALUE_WILDCARD -9999

static int normalize_var(bdd_dictionary* dict, dict_var* varp) { 
    double total = 0.0;

    for(int i=varp->offset; i<(varp->offset+varp->card); i++) {
        dict_val* valp = V_dict_val_getp(dict->values,i);
        total += valp->prob;
    }
    if ( total<(1.0-0.0001) || total>(1.0+0.0001) ) {
        double factor = 1.0/total;
        for(int i=varp->offset; i<(varp->offset+varp->card); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            valp->prob *= factor;
        }
    }
    return 1;
}

int modify_dictionary(bdd_dictionary* dict, dict_mode mode, char* dictionary_def, char** _errmsg) {
    dict_var* varp = NULL;
    char *p = dictionary_def;
    while ( *p ) {
        char*  scan_var     = NULL;
        int    scan_var_len = -1;
        int    scan_val     = -1;
        double scan_prob    = -1;


        while ( *p && !isalnum(*p) )
            p++;
        if ( isalnum(*p) ) {
            scan_var = p;
            while ( isalnum(*p) )
                p++;
            scan_var_len = p - scan_var;
            while ( *p && *p != '=' )
                p++;
            if ( !(*p++ == '=') )
                return pg_error(_errmsg,"modify_dictionary: missing \'=\' in dictionay def: \"%s\"",scan_var);

            while ( isspace(*p) ) p++;
            if ( *p == '*' ) {
                if ( mode == DICT_DEL )
                    scan_val = VALUE_WILDCARD;
                else
                    return pg_error(_errmsg,"modify_dictionary:value wildcard * can only be used for delete : %s",scan_var);
            } else if ( isdigit(*p) ) {
                if ( (scan_val = bdd_atoi(p)) < 0 )
                    return pg_error(_errmsg,"modify_dictionary: bad integer value : \"%s\"",p);
            } else 
                return pg_error(_errmsg,"modify_dictionary: missing value after \'=\' in expr: \"%s\"",scan_var);

            p++;
            while (isdigit(*p) )
                p++;
            // 
            scan_prob = -1;
            if ( mode != DICT_DEL ) {
                while ( isspace(*p) ) p++;
                if ( !(*p++ == ':') )
                    return pg_error(_errmsg,"modify_dictionary: missing \':\' in dictionay def: \"%s\"",scan_var);
                while ( isspace(*p) ) p++;
                char * endptr;
                scan_prob = strtod(p, &endptr);
                if ( p == endptr )
                    return pg_error(_errmsg,"modify_dictionary: bad probability value : \"%s\"",p);
                p = endptr;
            } else {
                // check a common error :-)
                while ( isspace(*p) ) p++;
                if (*p == ':')
                    return pg_error(_errmsg,"modify_dictionary: unexpected \':\' in delete: \"%s\"",scan_var);
            }
            while ( isspace(*p) ) p++;
            if ( *p == ';' ) {
                p++;
                while ( isspace(*p) ) p++;
            }
        }
        if ( scan_var_len > MAX_RVA_NAME )
            return pg_error(_errmsg,"modify_dictionary: varname too long, max(%d) : %s",MAX_RVA_NAME,scan_var);
        char varname[MAX_RVA_NAME+1];
        memcpy(varname,scan_var,scan_var_len);
        varname[scan_var_len] = 0;
        // fprintf(stderr,"SCANNED: [%d] %s=%d : %f;\n",mode,varname,scan_val,scan_prob);
        if ( varp && (strcmp(varp->name,varname) !=0) ) {
                normalize_var(dict,varp); // start with a new var
                varp = NULL;
            }
        if ( varp == NULL ) {
            varp = bdd_dictionary_lookup_var(dict,varname);
            if ( varp == NULL ) {
                if (mode == DICT_ADD) {
                    if ( !(varp = new_var(dict,varname)) ) {
                        return pg_error(_errmsg,"modify_dictionary:error creating var \"%s\"",varname);
                    }
                } else
                    return pg_error(_errmsg,"modify_dictionary:upd/del: unknown var \"%s\"",varname);
            } 
        }
        int vvi;
        if ( mode == DICT_ADD || mode == DICT_UPD ) {
            vvi = get_var_value_index(dict,varp,scan_val);
            if ( vvi < 0 ) { // no variable/value
                if ( mode == DICT_ADD ) {
                    // first check if the varp values are at tail of 'values',
                    // if not create a value 'hole' and copy all varp val/prob
                    // values to end of 'values' and add the new value.
                    if (!((varp->offset+varp->card)==V_dict_val_size(dict->values))) {
                        int prev_offset    = varp->offset;
                        varp->offset       = V_dict_val_size(dict->values);
                        for (int i =0; i<varp->card; i++) {
                        if ( !new_val(dict,dict->values->items[prev_offset+i].value,dict->values->items[prev_offset+i].prob)  )
                            return pg_error(_errmsg,"modify_dictionary:add: internal error reorganzing values");
                        }
                        dict->val_deleted += varp->card; // 'hole' size
                    }
                    if ( !new_val(dict,scan_val,scan_prob)  )
                        return pg_error(_errmsg,"modify_dictionary:add: internal error %s=%f ",varname,scan_var,scan_prob);
                    varp->card++;
                } else {
                    return pg_error(_errmsg,"modify_dictionary:add: variable/value %s=%d does not exist ",varname,scan_val);
                }
            } else {
                if ( mode == DICT_ADD ) {
                    return pg_error(_errmsg,"modify_dictionary:add: variable/value %s=%d alreay exists",varname,scan_val);
                } else {
                    dict_val* valp = V_dict_val_getp(dict->values,vvi);
                    valp->prob = scan_prob;
                }
            }
        } else { // mode == DICT_DEL
            if ( scan_val == VALUE_WILDCARD ) {
                // delete entire var
                dict->val_deleted += varp->card;
                int var_index = lookup_var_index(dict,varname);
                V_dict_var_delete(dict->variables,var_index);
                varp = NULL; // to prevent normalization
            } else {
                // delete one rva
                if ( (vvi=get_var_value_index(dict,varp,scan_val)) < 0)
                    return pg_error(_errmsg,"modify_dictionary:del: variable/value %s=%d does not exits ",varname,scan_val);
                dict->val_deleted++;
                if ( --varp->card == 0) { // delete entire var
                    dict->val_deleted += varp->card;
                    int var_index = lookup_var_index(dict,varname);
                    V_dict_var_delete(dict->variables,var_index);
                    varp = NULL; // to prevent normalization
                } else {
                    // V_dict_val_copy_range(dict->values,vvi,varp->offset+varp->card-vvi-1,vvi+1);
                    for(int i=vvi; i<(varp->offset+varp->card); i++) {
                        dict->values->items[i] = dict->values->items[i+1]; // INCOMPLETE: UGLY!!!!
                    }
                }
            }
        }
    }
    if ( varp )
        normalize_var(dict,varp);
    return 1;
}
