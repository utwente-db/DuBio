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

#include "postgres.h"
#include "funcapi.h"

#ifndef MYSTR_DATATYPES
/** 
 * Prevent this header from being included multiple times.
 */
#define MYSTR_DATATYPES 1

typedef struct Mystr {
        char    vl_len[4];
        size_t  size;
        char    buff[0];
} Mystr;

/**
 * Provide a macro for getting a bitmap datum.
 */
#define DatumGetMystr(x)	((Mystr *) DatumGetPointer(x))

/**
 * Provide a macro for dealing with bitmap arguments.
 */
#define PG_GETARG_MYSTR(x)	DatumGetMystr(		\
		PG_DETOAST_DATUM(PG_GETARG_DATUM(x)))
/**
 * Provide a macro for returning bitmap results.
 */
#define PG_RETURN_MYSTR(x)	PG_RETURN_POINTER(x)

/**
 * Defines a boolean type to make our code more readable.
 */
typedef unsigned char boolean;

extern Datum mystr_in(PG_FUNCTION_ARGS);
extern Datum mystr_out(PG_FUNCTION_ARGS);
extern Datum mystr_add(PG_FUNCTION_ARGS);
extern Datum mystr_equal(PG_FUNCTION_ARGS);
extern Datum mystr_nequal(PG_FUNCTION_ARGS);

#endif
