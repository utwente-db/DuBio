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
#ifndef PG_CONFIG_H
#define PG_CONFIG_H

#include "postgres.h"
#include "funcapi.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/numeric.h"

#define PG_CONFIG

#define MALLOC  palloc
#define REALLOC repalloc
#define FREE    pfree

//

#define DatumGetDictionary(x)        bdd_dictionary_relocate(((bdd_dictionary *) x))

#define PG_GETARG_DICTIONARY(x)      DatumGetDictionary(          \
                PG_DETOAST_DATUM(PG_GETARG_DATUM(x)))

#define PG_RETURN_DICTIONARY(x)      PG_RETURN_POINTER(x)



#define DatumGetDictionaryRef(x)     ((bdd_dictionary_ref *) DatumGetPointer(x))
#define PG_GETARG_DICTIONARY_REF(x)  DatumGetDictionaryRef(PG_GETARG_DATUM(x))
#define PG_RETURN_DICTIONARY_REF(x)  PG_RETURN_POINTER(x)

//

#define DatumGetBdd(x)        relocate_bdd(((bdd *) x))

#define PG_GETARG_BDD(x)      DatumGetBdd(          \
                PG_DETOAST_DATUM(PG_GETARG_DATUM(x)))

#define PG_RETURN_BDD(x)      PG_RETURN_POINTER(x)

//
//
//

/*
 * A couple of pg_* utility functions used interfacing with Postgres. The are
 * implemented in pg_utils.c
 */

typedef struct pbuff pbuff; // forward, defined in utils.h
typedef struct bdd   bdd;   // ,,

char* pbuff2cstring(pbuff* pbuff, int maxsz);
text* pbuff2text(pbuff* pbuff, int maxsz);

text* bdd2text(bdd* bdd, int encapsulate);
char* bdd2cstring(bdd* bdd, int encapsulate);

//
//
//

extern Datum dictionary_in(PG_FUNCTION_ARGS);
extern Datum dictionary_out(PG_FUNCTION_ARGS);
extern Datum dictionary_print(PG_FUNCTION_ARGS);

#endif
