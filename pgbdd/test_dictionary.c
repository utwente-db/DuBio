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
    char* errmsg;

    char* input = "x=0 : 0.6; x=1 : 0.2; x=2 : 0.1; x=3 : 0.1; y=3 : 0.5; y=1 : 0.2; y=2 : 0.3; q=8 : 1.0; ";
    dict = bdd_dictionary_create(&dict_struct,"XYZ");
    if (bdd_dictionary_addvars(dict,input,0/*update*/,&errmsg)) {
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
    bdd_dictionary dict_struct, *d;
    char* errmsg;

    d = bdd_dictionary_create(&dict_struct,"mydict");
    //
    if ( !bdd_dictionary_addvars(d,"x=0 : 0.6; x=1 : 0.4;  ",0/*update*/,&errmsg) ) 
        { fprintf(stderr,"%s",errmsg); return 0; }
    bdd_dictionary_print(d,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    if ( !bdd_dictionary_addvars(d,"y=0:0.5;y=1:0.5; d=0:0.8;d=1:0.1;d=2:0.1;",0/*update*/,&errmsg) ) 
        { fprintf(stderr,"%s",errmsg); return 0; }
    bdd_dictionary_sort(d);
    bdd_dictionary_print(d,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    if ( !bdd_dictionary_delvars(d,"d=0; ",&errmsg) ) 
        { fprintf(stderr,"%s",errmsg); return 0; }
    bdd_dictionary_print(d,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    if ( !bdd_dictionary_delvars(d,"d=*; ",&errmsg) ) 
        { fprintf(stderr,"%s",errmsg); return 0; }
    bdd_dictionary_print(d,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    if ( !bdd_dictionary_addvars(d,"y=0 : 1.0; ",1/*update*/,&errmsg) ) 
        { fprintf(stderr,"%s",errmsg); return 0; }
    bdd_dictionary_print(d,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    bdd_dictionary_free(d);
    //
    return 1;
}

int test_dictionary() {
    return test_bdd_dictionary_v1();
}
