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

#include "mystr.h"

PG_FUNCTION_INFO_V1(mystr_equal);
/** 
 * <code>mystr_equal(mystr1 mystr, mystr2 mystr) returns bool</code>
 * Return true if the mystrs are equivalent,
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr1 mystr</code> The first mystr
 * <br><code>mystr2 mystr</code> The second mystr
 * @return <code>bool</code> true if the mystrs are the same expression
 */
Datum
mystr_equal(PG_FUNCTION_ARGS)
{
        Mystr *mystr1;
        Mystr *mystr2;

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1)) {
		PG_RETURN_NULL();
	}
        mystr1 = PG_GETARG_MYSTR(0);
        mystr2 = PG_GETARG_MYSTR(1);

	PG_RETURN_BOOL( (mystr1 == mystr2) );
}


PG_FUNCTION_INFO_V1(mystr_nequal);
/** 
 * <code>mystr_nequal(mystr1 mystr, mystr2 mystr) returns bool</code>
 * Return false if the mystrs are equivalent,
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr1 mystr</code> The first mystr
 * <br><code>mystr2 mystr</code> The second mystr
 * @return <code>bool</code> false if the mystrs are the same expression
 */
Datum
mystr_nequal(PG_FUNCTION_ARGS)
{
        Mystr *mystr1;
        Mystr *mystr2;

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1)) {
		PG_RETURN_NULL();
	}
        mystr1 = PG_GETARG_MYSTR(0);
        mystr2 = PG_GETARG_MYSTR(1);

	PG_RETURN_BOOL( !(mystr1 == mystr2) );
}
