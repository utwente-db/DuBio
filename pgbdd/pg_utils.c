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

#include "pg_config.h"

#include "utils.c"

#include "dictionary.h"
#include "bdd.h"

/*
 *
 *
 */

text* pbuff2text(pbuff* pbuff, int maxsz) {
    /*
     * This function converts the contents of a pbuff to a postgresql 
     * TEXT VALUE. After this the palloc()'ed memory used by the pbuff
     * is freed.
     */
    text* res = cstring_to_text_with_len(pbuff->buffer,pbuff->size);
    pbuff_free(pbuff);
    return res;
}

char* pbuff2cstring(pbuff* pbuff, int maxsz) {
    /*
     * This function converts the contents of a pbuff to a postgresql 
     * CSTRING VALUE. After this the palloc()'ed memory used by the pbuff
     * is freed.
     */
    char* res = pbuff_preserve_or_alloc(pbuff);
    return res;
}

text* bdd2text(bdd* bdd, int encapsulate) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd2string(pbuff,bdd,encapsulate);
    return pbuff2text(pbuff,PBUFF_MAX_TOTAL);
}

char* bdd2cstring(bdd* bdd, int encapsulate) {
    pbuff pbuff_struct, *pbuff=pbuff_init(&pbuff_struct);
    bdd2string(pbuff,bdd,encapsulate);
    return pbuff2cstring(pbuff,-1);
}

