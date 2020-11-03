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


#define MAX_RVA_NAME    12
#define MAX_RVA_LEN     24

typedef struct dict_var {
    char name[MAX_RVA_NAME];
    int  offset;
    int  card;
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

#define MAX_VAL_DELETED 128   /* reorganize above this max number */

typedef struct bdd_dictionary {
    char        vl_len[4]; // used by Postgres memory management
    int         magic;
    int         bytesize;
    int         n_justcopy;
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

bdd_dictionary* bdd_dictionary_create(bdd_dictionary*);
int bdd_dictionary_free(bdd_dictionary*);
bdd_dictionary* dictionary_prepare2store(bdd_dictionary*);
bdd_dictionary* bdd_dictionary_relocate(bdd_dictionary*);
void bdd_dictionary_print(bdd_dictionary* dict, int all, pbuff* pbuff);
int bdd_dictionary_sort(bdd_dictionary* dict);
bdd_dictionary* merge_dictionary(bdd_dictionary*,bdd_dictionary*,bdd_dictionary*,char** _errmsg);

#define DICT_ADD        1
#define DICT_DEL        2
#define DICT_UPD        3

#define IS_VALID_MODIFIER(M)    (((M)>0)&&((M)<4))

int modify_dictionary(bdd_dictionary*, int, char*, char**);

typedef struct rva rva; /* forward */

double lookup_probability(bdd_dictionary*,rva*);

int test_bdd_dictionary_v0(void);
int test_bdd_dictionary_v1(void);
int test_bdd_dictionary_v2(void);

int test_dictionary(void);

#endif
