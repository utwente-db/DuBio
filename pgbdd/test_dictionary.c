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

#include "dictionary.c"

int test_bdd_dictionary_v0() {
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
    return 1;
}

int test_bdd_dictionary_v1() {
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
    return 1;
}

int test_bdd_dictionary_v2() {
    char* name    = "TestDictionary";
    char* initvars = "x=0 : 0.6; x=1 : 0.2; x=2 : 0.1; x=3 : 0.1; y=3 : 0.2; y=1 : 0.2; y=2 : 0.1; q=8 : 0.5; p=5:0.5; p=6:0.5;";
    bdd_dictionary new_dict_struct, *new_dict;

    if ( !(new_dict = bdd_dictionary_create(&new_dict_struct,name)) ) 
        return vector_error("DICTIONARY CREATE[%s] failed: %s",name,initvars);
    if (0) if ( !bdd_dictionary_addvars(new_dict,initvars))
        return vector_error("DICTIONARY ADDVARS[%s] failed: %s",name,initvars);
    bdd_dictionary* return_dict = bdd_dictionary_serialize(new_dict);
    bdd_dictionary_free(new_dict);
    bdd_dictionary_sort(return_dict);
    if ( 1 ) {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);

        bdd_dictionary_print(return_dict, pbuff);
        pbuff_flush(pbuff,stdout);
        pbuff_preserve_or_alloc(pbuff); // return for db calls
    }
    return 1;
}

int test_bdd_dictionary() {
    test_bdd_dictionary_v0();
    test_bdd_dictionary_v1();
    return test_bdd_dictionary_v2();
}
