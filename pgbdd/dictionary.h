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


#define MAX_RVA_NAME_BUFF    12
#define MAX_RVA_NAME         (MAX_RVA_NAME_BUFF-1)        
#define MAX_RVA_LEN          24

typedef unsigned short       dindex;
#define MAX_DINDEX           USHRT_MAX

typedef struct dict_var {
    char    name[MAX_RVA_NAME_BUFF];
    dindex  offset;
    dindex  card;
} dict_var;

DefVectorH(dict_var);

int cmpDict_var(dict_var* l, dict_var* r);

//

typedef struct dict_val {
    double  prob;        // Why double, with float sizeof(dict_val)=8 <> 16
    int    value;
} dict_val;

DefVectorH(dict_val);

//

#define MAX_VAL_DELETED 128   /* reorganize above this max number */

typedef struct bdd_dictionary {
    char        vl_len[4]; // used by Postgres memory management
    int         bytesize;
    dindex      val_deleted;
    dindex      var_offset; // first in buff so is always 0 :-)
    char        var_sorted; // when sorted use binary search to find var
    V_dict_var* variables;
    int         val_offset; // after the vars, so size of var buffer
    V_dict_val* values;
    char        buff[sizeof(V_dict_var)+sizeof(V_dict_val)];   
} bdd_dictionary;

#define BDD_DICTIONARY_BASESIZE (sizeof(bdd_dictionary)-(sizeof(V_dict_var)+sizeof(V_dict_val)))

//

bdd_dictionary* bdd_dictionary_create(bdd_dictionary*);
int             bdd_dictionary_free(bdd_dictionary*);

bdd_dictionary* bdd_dictionary_relocate(bdd_dictionary*);
bdd_dictionary* dictionary_prepare2store(bdd_dictionary*);
void            bdd_dictionary_print(bdd_dictionary* dict,int all,pbuff* pbuff);
bdd_dictionary* merge_dictionary(bdd_dictionary*,bdd_dictionary*,bdd_dictionary*,char** _errmsg);

#define DICT_ADD        1
#define DICT_DEL        2
#define DICT_UPD        3

#define IS_VALID_MODIFIER(M)    (((M)>0)&&((M)<4))

int modify_dictionary(bdd_dictionary*, int, char*, char**);

typedef struct rva rva; /* forward */

double lookup_probability(bdd_dictionary*,rva*);
int lookup_alternatives(bdd_dictionary* dict,char* var, pbuff* pbuff, char** _errmsg);

int test_dictionary(void);

/*
 *
 */

# define BDR_MAGIC 8112261925143664796

typedef struct bdd_dictionary_ref {
    long            magic;
    bdd_dictionary* ref;
} bdd_dictionary_ref;

bdd_dictionary*     get_dict_from_ref(bdd_dictionary_ref* bdr, char** _errmsg);
bdd_dictionary_ref* create_ref_from_dict(bdd_dictionary* dict, char** _errmsg);

#endif
