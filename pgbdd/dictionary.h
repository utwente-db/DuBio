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
#ifndef DICTIONARY_H
#define DICTIONARY_H

typedef struct dict_var {
    char name[MAXRVA];
    int  offset;
    int  cardinality;
} dict_var;

DefVectorH(dict_var);

int cmpDict_var(dict_var* l, dict_var* r);

//

typedef struct dict_val {
    int    value;
    double prob;
} dict_val;

DefVectorH(dict_val);

//

typedef struct bdd_dictionary {
    char        vl_len[4]; // used by Postgres memory management
    char        name[MAXRVA];
    int         magic;
    int         size;
    int         val_deleted;
    int         var_sorted; // when sorted use binary search to find var
    int         var_offset; // first in buff so is always 0 :-)
    V_dict_var* variables;
    int         val_offset; // after the vars, so size of var buffer
    V_dict_val* values;
    char        buff[sizeof(V_dict_var)+sizeof(V_dict_val)];   
} bdd_dictionary;

#define BDD_DICTIONARY_BASESIZE (sizeof(bdd_dictionary)-(sizeof(V_dict_var)+sizeof(V_dict_val)))

//

bdd_dictionary* bdd_dictionary_create(bdd_dictionary*,char*);
int bdd_dictionary_free(bdd_dictionary*);
bdd_dictionary* bdd_dictionary_serialize(bdd_dictionary*);
dict_var* bdd_dictionary_lookup_var(bdd_dictionary*, char*);
int bdd_dictionary_is_serialized(bdd_dictionary* dict);
bdd_dictionary* bdd_dictionary_relocate(bdd_dictionary* dict);
void bdd_dictionary_print(bdd_dictionary* dict, pbuff* pbuff);
int lookup_var_index(bdd_dictionary* dict, char* name);
int bdd_dictionary_sort(bdd_dictionary* dict);
int bdd_dictionary_delvars(bdd_dictionary*, char*, char**);
int bdd_dictionary_addvars(bdd_dictionary*,char*,int,char**);


int test_bdd_dictionary_v0(void);
int test_bdd_dictionary_v1(void);

int test_bdd_dictionary(void);

#endif
