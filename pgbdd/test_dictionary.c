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

static int test_bdd_dictionary_v0() {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_dictionary dict_struct, *dict;
    char* errmsg;

    char* input = "x=0 : 0.6; x=1 : 0.2; x=2 : 0.1; x=3 : 0.1; y=3 : 0.5; y=1 : 0.2; y=2 : 0.3; qq_1=8 : 1.0; ";
    if ( !(dict = bdd_dictionary_create(&dict_struct) ))
        return 0;;
    if (modify_dictionary(dict,DICT_ADD,input,&errmsg)) {
        bdd_dictionary *new_dict, *relocate_dict;

        bdd_dictionary_print(dict,0/*all*/,pbuff);
        if ( 0 ) {
            fprintf(stdout,"*** Dictionary v0 contents ***\n");
            pbuff_flush(pbuff,stdout);
            fprintf(stdout,"******************************\n");
        }
        new_dict = bdd_dictionary_serialize(dict);
        bdd_dictionary_free(dict);
        bdd_dictionary_sort(new_dict);
        bdd_dictionary_lookup_var(new_dict,"x");
        bdd_dictionary_print(new_dict,0/*all*/,pbuff);
        // pbuff_flush(pbuff,stdout);
        relocate_dict = (bdd_dictionary*)MALLOC(new_dict->bytesize);
        memcpy(relocate_dict,new_dict,new_dict->bytesize);
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

static void td(bdd_dictionary *d,int mode,char* modifiers) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char* _errmsg;
   
    fprintf(stdout,"+ modify_dict[%d]: %s\n",mode,modifiers);
    if ( !modify_dictionary(d,mode,modifiers,&_errmsg) ) { 
        fprintf(stderr,"Dictionary Error: %s",_errmsg); exit(0);
    }
    bdd_dictionary_print(d,0/*all*/,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
}

static void tdmerge(char* ld_add, char* rd_add) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    char* _errmsg;
    bdd_dictionary ldict, *ld;
    bdd_dictionary rdict, *rd;
    bdd_dictionary mdict, *md;

    if ( !(ld = bdd_dictionary_create(&ldict)))
        {fprintf(stderr,"Dictionary create failed\n"); exit(0);}
    if ( !modify_dictionary(ld,DICT_ADD,ld_add,&_errmsg) ) { 
        fprintf(stderr,"Dictionary Error: %s",_errmsg); exit(0);
    }
    fprintf(stdout,"+ ld: \n");
    bdd_dictionary_print(ld,0/*all*/,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    if ( !(rd = bdd_dictionary_create(&rdict)))
        {fprintf(stderr,"Dictionary create failed\n"); exit(0);}
    if ( !modify_dictionary(rd,DICT_ADD,rd_add,&_errmsg) ) { 
        fprintf(stderr,"Dictionary Error: %s",_errmsg); exit(0);
    }
    fprintf(stdout,"+ rd: \n");
    bdd_dictionary_print(rd,0/*all*/,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    if ( !(md = merge_dictionary(&mdict,ld,rd,&_errmsg))) {
        fprintf(stderr,"Dictionary merge:error: %s",_errmsg); exit(0); }
    fprintf(stdout,"+ md: \n");
    bdd_dictionary_print(md,0/*all*/,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
}

static int test_bdd_dictionary_v2() {
    tdmerge("y=1:1.0;","x=2:1.0;y=2:1.0");
    tdmerge("p=1:1.0;r=1:1.0","q=2:1.0;s=2:1.0");
    tdmerge("p=1:1.0;r=1:1.0;q=2:1.0;s=2:1.0","p=3:1.0;r=4:1.0;q=8:1.0;s=9:1.0");
    tdmerge("p=1:1.0;r=4:1.0;q=2:1.0;s=2:1.0","p=3:1.0;r=4:0.5;r=5:0.5;s=9:1.0");
    return 1;
}

static int test_bdd_dictionary_v1() {
    bdd_dictionary dict_struct, *first_dict, *d;
    if ( !(first_dict = bdd_dictionary_create(&dict_struct)))
        return 0;
    if ( !(d = bdd_dictionary_serialize(first_dict)))
        return 0;
    bdd_dictionary_free(first_dict);
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
    if ( 1 && !(d = dictionary_prepare2store(d)))
        return 0;
    if ( 1 ) {
        pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
        bdd_dictionary_print(d,1/*all*/,pbuff);
        pbuff_flush(pbuff,stdout);
    }
    //
    bdd_dictionary_free(d);
    FREE(d);
    //
    return 1;
}

static char* load_file(char const* path)
{
    char* buffer = 0;
    long length = -1;
    FILE * f = fopen (path, "rb"); //was "rb"

    if (f)
    {
      fseek (f, 0, SEEK_END);
      length = ftell (f);
      fseek (f, 0, SEEK_SET);
      buffer = (char*)malloc ((length+1)*sizeof(char));
      if (buffer)
      {
        fread (buffer, sizeof(char), length, f);
      }
      fclose (f);
    } else {
      fprintf(stderr,"Unable to open file: %s\n",path);
      exit(-1);
    }
    buffer[length] = '\0';
    return buffer;
}

static int test_bdd_dictionary_v3() {
    char *_errmsg;
    char *input;
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd_dictionary dict_struct, *dict, *d;

    fprintf(stdout,"Testing BIG dictionary start\n");
    if ( !(dict = bdd_dictionary_create(&dict_struct)))
        return 0;
    // char* input = "x=1:0.6; x=2:0.4";
    input = load_file("/Users/flokstra/dictstring_val.txt");
    // td(dict,DICT_ADD,input);
    //
    if ( !modify_dictionary(dict,DICT_ADD,input,&_errmsg) ) {
        fprintf(stderr,"Dictionary Error: %s",_errmsg); exit(0);
    }
    bdd_dictionary_print(dict,0/*all*/,pbuff);pbuff_flush(pbuff,stdout);fputc('\n',stdout);
    //
    if ( 1 && !(d = dictionary_prepare2store(dict)))
        return 0;
    bdd_dictionary_free(d);
    FREE(d);
    fprintf(stdout,"Testing BIG dictionary finish\n");
    //
    return 1;
}

int test_dictionary() {
    if (1) test_bdd_dictionary_v0();
    if (0) test_bdd_dictionary_v1();
    if (0) test_bdd_dictionary_v2();
    if (0) test_bdd_dictionary_v3();
    return 1;
}
