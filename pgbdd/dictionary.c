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

#include <limits.h>
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

//
//
//

static int bdd_dictionary_is_serialized(bdd_dictionary* dict) {
    if (  V_dict_var_is_serialized(dict->variables) &&
          V_dict_val_is_serialized(dict->values) ) {
        if ( (((void*)dict->variables > (void*)dict) && ((void*)(dict->variables+0) <= (void*)(dict+dict->bytesize))) &&
              (((void*)dict->values > (void*)dict) && ((void*)(dict->values+0) <= (void*)(dict+dict->bytesize))))
             return 1;
        else 
             pg_fatal("bdd_dictionary_is_serialized: unexpected vectors outside dict");
    }
    return 0;
}

static bdd_dictionary* bdd_dictionary_serialize(bdd_dictionary* dict) {
    bdd_dictionary* res = NULL;

    if ( bdd_dictionary_is_serialized(dict) ) {
        if ( !(res  = (bdd_dictionary*)MALLOC(dict->bytesize)) )
            return NULL;
        memcpy(res,dict,dict->bytesize);
        if ( !bdd_dictionary_relocate(res) )
            pg_fatal("bdd_dictionary_serialize: relocate error");
    } else {
        int varsize = V_dict_var_bytesize(dict->variables);
        int valsize = V_dict_val_bytesize(dict->values);
        int newsize = BDD_DICTIONARY_BASESIZE + varsize + valsize;
        if ( !(res  = (bdd_dictionary*)MALLOC(newsize)) )
            return NULL;
        memcpy(res,dict,BDD_DICTIONARY_BASESIZE);
        res->bytesize = newsize;
        res->var_offset = 0;
        res->variables  = V_dict_var_serialize((void*)(&res->buff[res->var_offset]),dict->variables);
        res->val_offset = varsize;
        res->values     = V_dict_val_serialize((void*)(&res->buff[res->val_offset]),dict->values);
    }
    return res;
}

static int bdd_dictionary_sort(bdd_dictionary* dict) {
    if ( !dict->var_sorted ) {
        V_dict_var_quicksort(dict->variables,cmpDict_var);
        dict->var_sorted = 1;
    }
    return 1;
}

bdd_dictionary* dictionary_prepare2store(bdd_dictionary* dict) {
    bdd_dictionary *res = NULL;

    if ( bdd_dictionary_sort(dict) && (res=bdd_dictionary_serialize(dict))) {
        /* check if the values Array must be reorganized. During the update
         * of the dictionary value holes may have been created. Reserializing
         * the values from the old 'dict' to the new 'res' dict may remove
         * these holes.
         */
        if ( dict->val_deleted > MAX_VAL_DELETED ) {
            dindex newcnt = 0;
            for(int i=0; i<V_dict_var_size(dict->variables); i++) {
                dict_var* old_varp = V_dict_var_getp(dict->variables,i);
                dict_var* new_varp = V_dict_var_getp( res->variables,i);
                
                new_varp->offset = newcnt;
                for(dindex j=old_varp->offset; j<(old_varp->offset+old_varp->card); j++) {
                    dict_val* valp = V_dict_val_getp(dict->values,j);
                    V_dict_val_set(res->values,newcnt++,*valp);
                }
            }
            res->values->size = (int)newcnt; // ugly but it works
            // incomplete, check if capacity should be decreased
        }
        /* The dict was the parameter from Postgres. It was serialized in the
         * beginning but may have been enlarged. The next free statement free's
         * only these new enlargements which were 'outside' the serialized dict.
         * Do not remove 'dict' itself, this is pfreed() by Postgres.
         */
        bdd_dictionary_free(dict);
    }
    return res;
}

bdd_dictionary* bdd_dictionary_create(bdd_dictionary* dict) {
    // incomplete, randomize for time ???
    dict->bytesize    = sizeof(bdd_dictionary);
    dict->var_sorted  = 0;
    dict->var_offset  = 0;
    dict->val_deleted = 0;
    dict->variables   = V_dict_var_init((V_dict_var*)(&dict->buff[dict->var_offset]));
    dict->val_offset  = sizeof(V_dict_var);
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
    dict->variables = NULL;
    dict->values    = NULL;
    return 1;
}

void bdd_dictionary_print(bdd_dictionary* dict, int all, pbuff* pbuff) {
    if ( all ) {
        bprintf(pbuff,"# variables[size/cap]= [%d/%d]\n",dict->variables->size,dict->variables->capacity);
        bprintf(pbuff,"# values[size/cap]   = [%d/%d]\n",dict->values->size,dict->values->capacity);
        bprintf(pbuff,"# bytesize=%d\n",dict->bytesize);
        bprintf(pbuff,"# sorted=%d\n",dict->var_sorted);
        bprintf(pbuff,"# val_deleted=%d\n",dict->val_deleted);
        bprintf(pbuff,"# serialized=%d\n",bdd_dictionary_is_serialized(dict));
    }
    if ( all ) {
        for(int i=0; i<V_dict_var_size(dict->variables); i++) {
            dict_var* varp = V_dict_var_getp(dict->variables,i);
            bprintf(pbuff,"(%d)\t\"%s\"\t off(%d)\t card(%d)\n",i,varp->name,(int)varp->offset,(int)varp->card);
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
            for(dindex j=varp->offset; j<(varp->offset+varp->card); j++) {
                dict_val* valp = V_dict_val_getp(dict->values,j);
                bprintf(pbuff,"%s=%d:%f; ",varp->name,valp->value,valp->prob);
            }
            bprintf(pbuff,"\n");
        }
    }
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

static dict_var* add_variable(bdd_dictionary* dict, char* name) {
    int index;
    dict_var newvar = { .offset=(dindex)V_dict_val_size(dict->values), .card=0 };
    strcpy(newvar.name,name);

    dict->var_sorted = 0; // INCOMPLETE, MAYBE KEEP SORTED LIKE BDD
    index = V_dict_var_add(dict->variables,&newvar);
    if ( (index < 0) || (index >= MAX_DINDEX) )
        return NULL;
    else
        return V_dict_var_getp(dict->variables,index);
}

static void del_variable(bdd_dictionary* dict, int var_index) {
     // dict->var_sorted is unchanged!
     V_dict_var_delete(dict->variables,var_index);
}

static dict_val* add_value(bdd_dictionary* dict, int value, double prob) {
    dict_val newval = { .value = value, .prob = prob };

    /* This is the only place where ushort with small footprint should be
     * checked. There is no other way to create larger offset/card values 
     * than 65536. Variables need at last 1 value so maybe they donot have
     * to be checked at all
     */
    int index = V_dict_val_add(dict->values,&newval);
    if ( (index < 0) || (index >= MAX_DINDEX) )
        return NULL;
    else
        return V_dict_val_getp(dict->values,index);
}

static int get_var_value_index(bdd_dictionary* dict, dict_var* varp, int val) {
    if ( val >= 0 ) {
        for(dindex i=varp->offset; i<(varp->offset+varp->card); i++) {
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

int lookup_alternatives(bdd_dictionary* dict,char* var, pbuff* pbuff, char** _errmsg) {
    dict_var* varp = bdd_dictionary_lookup_var(dict, var);
    if ( !varp ) {
        return pg_error(_errmsg,"lookup_alternatives: unknown var: %s", var);
    } else {
        for(dindex i=varp->offset; i<(varp->offset+varp->card); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            if ( i > varp->offset )
                bprintf(pbuff,",");
            bprintf(pbuff,"%d",valp->value);
        }
    }
    return 1;
}

/*
 * Dictionary modification functions
 *
 */

#define VALUE_WILDCARD -9999

static int normalize_var(bdd_dictionary* dict, dict_var* varp) { 
    double total = 0.0;

    for(dindex i=varp->offset; i<(varp->offset+varp->card); i++) {
        dict_val* valp = V_dict_val_getp(dict->values,i);
        total += valp->prob;
    }
    if ( total<(1.0-0.0001) || total>(1.0+0.0001) ) {
        double factor = 1.0/total;
        for(dindex i=varp->offset; i<(varp->offset+varp->card); i++) {
            dict_val* valp = V_dict_val_getp(dict->values,i);
            valp->prob *= factor;
        }
    }
    return 1;
}

static int add2merged(bdd_dictionary *md,
                      dict_var       *srcvar,
                      V_dict_val     *srcvar_val,
                      V_dict_val     *merge_v,
                      dindex          merge_offset,
                      dindex          merge_card,
                      char**          _errmsg) {
    dict_var *mv;

    if ( !(mv = add_variable(md,srcvar->name)) )
        return 0;
    //
    for (dindex i=0; i<srcvar->card; i++) {
        dict_val *dval = V_dict_val_getp(srcvar_val,srcvar->offset+i);
        if ( !add_value(md,dval->value,dval->prob)  )
            return 0;
        mv->card++;
    }
    if ( merge_v ) {
        for (int i=0; i<merge_card; i++) {
            dict_val *dval = V_dict_val_getp(merge_v,merge_offset+i);
            int vindex = get_var_value_index(md,mv,dval->value);
            if (vindex >= 0) {
                dict_val *mval = V_dict_val_getp(md->values,vindex);
                if (dval->prob != mval->prob) 
                    return pg_error(_errmsg,"dict_merge: duplicate rva with conficting prob: %s=%d : %f <> %f",mv->name,dval->value,dval->prob,mval->prob);
            } else {
                if ( !add_value(md,dval->value,dval->prob)  )
                    return 0;
                mv->card++;
            }
        }
    }
    normalize_var(md,mv); // make sump of probabilities 1 again
    return 1;
}

bdd_dictionary* merge_dictionary(bdd_dictionary* md, bdd_dictionary* ld, bdd_dictionary* rd, char** _errmsg) {
    int lvars, rvars, lvals, rvals;
    int lvar_i, rvar_i;
    if ( !bdd_dictionary_create(md) )
        return NULL;
    if ( !(bdd_dictionary_sort(ld) && bdd_dictionary_sort(rd)) )
        return NULL;
    lvars = V_dict_var_size(ld->variables);
    rvars = V_dict_var_size(rd->variables);
    lvals = V_dict_val_size(ld->values) - ld->val_deleted;
    rvals = V_dict_val_size(rd->values) - rd->val_deleted;
    if ( !V_dict_var_resize(md->variables,lvars+rvars) )
        return NULL;;
    if ( !V_dict_val_resize(md->values,lvals+rvals) )
        return NULL;;
    lvar_i = 0; 
    rvar_i = 0;
    while ( lvar_i < lvars || rvar_i < rvars ) {
        if ( lvar_i == lvars ) { // no more left vars
            if ( !add2merged(md,V_dict_var_getp(rd->variables,rvar_i++),ld->values,NULL,-1,-1,_errmsg) )
                return 0;
        } else if ( rvar_i == rvars ) { // no more right vars
            if ( !add2merged(md,V_dict_var_getp(ld->variables,lvar_i++),ld->values,NULL,-1,-1,_errmsg) )
                return 0;
        } else {
            dict_var *lvar_p = V_dict_var_getp(ld->variables,lvar_i);
            dict_var *rvar_p = V_dict_var_getp(rd->variables,rvar_i);
            int cmp  = cmpDict_var(lvar_p,rvar_p);
            if ( cmp < 0) {
                if ( !add2merged(md,lvar_p,ld->values,NULL,-1,-1,_errmsg) )
                    return NULL;
                lvar_i++;
            } else if (cmp>0) {
                if ( !add2merged(md,rvar_p,rd->values,NULL,-1,-1,_errmsg) )
                    return NULL;
                rvar_i++;
            } else { // equal
                if ( !add2merged(md,lvar_p,ld->values,rd->values,lvar_p->offset,lvar_p->card,_errmsg) )
                    return NULL;
                lvar_i++;
                rvar_i++;
            }
        }
    }
    md->var_sorted = 1;
    return md;
}

#define VALID_VAR_CHAR(C)   (isalnum(C) || (C)=='_')

int modify_dictionary(bdd_dictionary* dict, int mode, char* dictionary_def, char** _errmsg) {
    dict_var* varp = NULL;
    char *p = dictionary_def;
    if ( !(IS_VALID_MODIFIER(mode)) )
        return pg_error(_errmsg,"modify_dictionary: bad modifier (%d)",mode);
    while ( *p ) {
        char*  scan_var     = NULL;
        int    scan_var_len = -1;
        int    scan_val     = -1;
        double scan_prob    = -1;
        char   varname[MAX_RVA_NAME+1];
        int    vvi;

        while ( *p && !VALID_VAR_CHAR(*p) )
            p++;
        if ( VALID_VAR_CHAR(*p) ) {
            scan_var = p;
            while ( VALID_VAR_CHAR(*p) )
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
                char *endptr;

                while ( isspace(*p) ) p++;
                if ( !(*p++ == ':') )
                    return pg_error(_errmsg,"modify_dictionary: missing \':\' in dictionay def: \"%s\"",scan_var);
                while ( isspace(*p) ) p++;
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
        memcpy(varname,scan_var,scan_var_len);
        varname[scan_var_len] = 0;
        // fprintf(stdout,"SCANNED: [%d] %s=%d : %f;\n",mode,varname,scan_val,scan_prob);
        if ( varp && (strcmp(varp->name,varname) !=0) ) {
                normalize_var(dict,varp); // start with a new var
                varp = NULL;
            }
        if ( varp == NULL ) {
            varp = bdd_dictionary_lookup_var(dict,varname);
            if ( varp == NULL ) {
                if (mode == DICT_ADD) {
                    if ( !(varp = add_variable(dict,varname)) ) {
                        return pg_error(_errmsg,"modify_dictionary:error creating var \"%s\"",varname);
                    }
                } else
                    return pg_error(_errmsg,"modify_dictionary:upd/del: unknown var \"%s\"",varname);
            } 
        }
        if ( mode == DICT_ADD || mode == DICT_UPD ) {
            vvi = get_var_value_index(dict,varp,scan_val);
            if ( vvi < 0 ) { // no variable/value
                if ( mode == DICT_ADD ) {
                    // first check if the varp values are at tail of 'values',
                    // if not create a value 'hole' and copy all varp val/prob
                    // values to end of 'values' and add the new value.
                    if (!((varp->offset+varp->card)==V_dict_val_size(dict->values))) {
                        dindex prev_offset    = varp->offset;
                        varp->offset       = (dindex)V_dict_val_size(dict->values);
                        for (dindex i =0; i<varp->card; i++) {
                        if ( !add_value(dict,dict->values->items[prev_offset+i].value,dict->values->items[prev_offset+i].prob)  )
                            return pg_error(_errmsg,"modify_dictionary:add: internal error reorganzing values");
                        }
                        dict->val_deleted += varp->card; // 'hole' size
                    }
                    if ( !add_value(dict,scan_val,scan_prob)  )
                        return pg_error(_errmsg,"modify_dictionary:add: internal error %s=%d:%f ",varname,scan_val,scan_prob);
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
                int var_index;
                // delete entire var
                dict->val_deleted += varp->card;
                var_index = lookup_var_index(dict,varname);
                del_variable(dict,var_index);
                varp = NULL; // to prevent normalization
            } else {
                // delete one rva
                if ( (vvi=get_var_value_index(dict,varp,scan_val)) < 0)
                    return pg_error(_errmsg,"modify_dictionary:del: variable/value %s=%d does not exists ",varname,scan_val);
                dict->val_deleted++;
                if ( --varp->card == 0) { // delete entire var
                    int var_index;
                    dict->val_deleted += varp->card;
                    var_index = lookup_var_index(dict,varname);
                    del_variable(dict,var_index);
                    varp = NULL; // to prevent normalization
                } else {
                    // V_dict_val_copy_range(dict->values,vvi,varp->offset+varp->card-vvi-1,vvi+1);
                    for(dindex i=vvi; i<(varp->offset+varp->card); i++) {
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

/*
 *
 */

bdd_dictionary_ref* create_ref_from_dict(bdd_dictionary* dict, char** _errmsg) {
    bdd_dictionary_ref* ref = NULL;

    // START_DP("/tmp/PG-DEBUG.TXT");
    if (!(ref=(bdd_dictionary_ref*)MALLOC(sizeof(struct bdd_dictionary_ref)))) {
        pg_error(_errmsg,"bdd_dictionary_ref: palloc failed");
        return NULL;
    }
    ref->magic = BDR_MAGIC;
    ref->ref   = dict; // warning, this dict should be in transaction valid storage
    DP("+ CREATE_REF called, id=%ld\n",(long)ref->ref);
    return ref;
}

bdd_dictionary* get_dict_from_ref(bdd_dictionary_ref* bdr, char** _errmsg) {
    // INCOMPLETE, there should also be a magic number in the dictionary
    // DP("+    GET_REF called, id=%ld\n",(long)bdr->ref);
    if ( bdr->magic != BDR_MAGIC ) {
        pg_error(_errmsg,"magic number not correct %ld",bdr->magic);
        return NULL;
    } else
        return bdr->ref;
}
