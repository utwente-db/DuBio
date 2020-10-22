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

#define PG_CONFIG

#define MALLOC  palloc
#define REALLOC repalloc
#define FREE    pfree

//

#define DatumGetDictionary(x)        bdd_dictionary_relocate(((bdd_dictionary *) DatumGetPointer(x)))

#define PG_GETARG_DICTIONARY(x)      DatumGetDictionary(          \
                PG_DETOAST_DATUM(PG_GETARG_DATUM(x)))

#define PG_RETURN_DICTIONARY(x)      PG_RETURN_POINTER(x)

//

#define DatumGetBdd(x)        relocate_bdd(((bdd *) DatumGetPointer(x)))

#define PG_GETARG_BDD(x)      DatumGetBdd(          \
                PG_DETOAST_DATUM(PG_GETARG_DATUM(x)))

#define PG_RETURN_BDD(x)      PG_RETURN_POINTER(x)

//
//
//

extern Datum dictionary_in(PG_FUNCTION_ARGS);
extern Datum dictionary_out(PG_FUNCTION_ARGS);
extern Datum dictionary_print(PG_FUNCTION_ARGS);
extern Datum dictionary_add(PG_FUNCTION_ARGS);
extern Datum dictionary_del(PG_FUNCTION_ARGS);
extern Datum dictionary_upd(PG_FUNCTION_ARGS);

#endif
