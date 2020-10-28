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
    if (modify_dictionary(dict,DICT_ADD,input,&errmsg)) {
        bdd_dictionary_print(dict,0/*all*/,pbuff);
        // pbuff_flush(pbuff,stdout);
        bdd_dictionary* new_dict = bdd_dictionary_serialize(dict);
        bdd_dictionary_free(dict);
        bdd_dictionary_sort(new_dict);
        bdd_dictionary_lookup_var(new_dict,"x");
        bdd_dictionary_print(new_dict,0/*all*/,pbuff);
        // pbuff_flush(pbuff,stdout);
        bdd_dictionary* relocate_dict = (bdd_dictionary*)MALLOC(new_dict->size);
        memcpy(relocate_dict,new_dict,new_dict->size);
        bdd_dictionary_relocate(relocate_dict);
        bdd_dictionary_free(new_dict);
        FREE(new_dict);
        bdd_dictionary_print(relocate_dict,0/*all*/,pbuff);
        // pbuff_flush(pbuff,stdout);
        bdd_dictionary_free(relocate_dict);
        FREE(relocate_dict);
    } else {
        fprintf(stdout,"DICTIONARY ADD VAR failed\n");
    }
    return 1;
}

static void td(bdd_dictionary *d,dict_mode mode,char* modifiers) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char* _errmsg;
   
    fprintf(stdout,"+ modify_dict[%d]: %s\n",mode,modifiers);
    if ( !modify_dictionary(d,mode,modifiers,&_errmsg) ) { 
        fprintf(stderr,"Dictionary Error: %s",_errmsg); exit(0);
    }
    bdd_dictionary_print(d,0/*all*/,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
}

int test_bdd_dictionary_v1() {
    bdd_dictionary dict_struct, *d;
    d = bdd_dictionary_create(&dict_struct,"mydict");
    //
    td(d,DICT_ADD,"x=1:0.6; x=2:0.4");
    td(d,DICT_ADD,"y=1:1.0;");
    td(d,DICT_ADD,"x=3:0.6; x=4:0.4");
    td(d,DICT_DEL,"x=2;x=4");
    td(d,DICT_ADD,"y=2:1.0;");
    td(d,DICT_ADD,"x=8:0.5; x=9:0.5");
    td(d,DICT_ADD,"z=1:0.5; z=2:0.5;z=3:0.5;z=4:0.5");
    td(d,DICT_UPD,"z=1:0.5; z=2:0.5");
    td(d,DICT_DEL,"y=1;y=2");
    td(d,DICT_DEL,"x=*");
    td(d,DICT_ADD,"x=1:0.6; x=2:0.4");
    //
    if ( 0 ) {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        bdd_dictionary_print(d,1/*all*/,pbuff);
        pbuff_flush(pbuff,stdout);
    }
    //
    bdd_dictionary_free(d);
    //
    return 1;
}

int test_dictionary() {
    test_bdd_dictionary_v0();
    test_bdd_dictionary_v1();
    return 1;
}
