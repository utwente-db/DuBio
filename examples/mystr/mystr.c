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

PG_MODULE_MAGIC;

static Mystr *
newMystr(char* str_expr) {
        size_t str_size = strlen(str_expr)+1;       
        size_t mystr_size = sizeof(Mystr) + str_size;
        Mystr* res = res = palloc(mystr_size);
        SET_VARSIZE(res,mystr_size); // !!!!! do not forget, cost me a day:-(
        res->size = str_size;
        strcpy(res->buff,str_expr);
        return res;
}

static Mystr *
addMystr(Mystr* l, Mystr* r) {
        size_t mystr_len = strlen(l->buff) +strlen(r->buff) + 1;
        char* resbuff = (char*)palloc(mystr_len);
        resbuff[0] = 0;
        strcpy(resbuff,l->buff);
        strcat(resbuff,r->buff);
        Mystr* res = newMystr(resbuff);
        pfree(resbuff);
        return res;
}

// static void 
// printMystr(Mystr* mystr) {
        // fprintf(stdout,"%s || %d\n",mystr->buff,(int)mystr->size);
// }
// 
// int main() {
        // Mystr* mystr = newMystr("Hello World");
        // printMystr(copyMystr(addMystr(newMystr("Hello"),newMystr(" World!"))));
        // return 0;
// }

PG_FUNCTION_INFO_V1(mystr_in);
/** 
 * <code>mystr_in(mystr_expression text) returns mystr</code>
 * Create a Mystr initialised from a text expression.
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr_expression text</code> A mystr expression
 * @return <code>Mystr</code> the newly created mystr
 */
Datum
mystr_in(PG_FUNCTION_ARGS)
{
	char    *mystr_expr;
        Mystr     *mystr;

        mystr_expr = PG_GETARG_CSTRING(0);
        mystr = newMystr(mystr_expr);

        PG_RETURN_MYSTR(mystr);
}


PG_FUNCTION_INFO_V1(mystr_out);
/** 
 * <code>mystr_out(mystr mystr) returns text</code>
 * Create a text representation of a mystr expression.
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr mystr</code> A mystr object
 * @return <code>cstring</code> the string representation of the expression
 */
Datum
mystr_out(PG_FUNCTION_ARGS)
{
        Mystr     *mystr;
	char    *result;

        mystr = PG_GETARG_MYSTR(0);
        result = palloc(strlen(mystr->buff)+1);
        strcpy(result,mystr->buff);

	PG_RETURN_CSTRING(result);
}

PG_FUNCTION_INFO_V1(mystr_add);
/** 
 * <code>mystr_add(mystr1 mystr, mystr2 mystr) returns mystr</code>
 * Return the concatenation of 2 mystr in a new mystr
 *
 * @param fcinfo Params as described_below
 * <br><code>mystr1 mystr</code> The first mystr
 * <br><code>mystr2 mystr</code> The second mystr
 * @return <code>mystr</code> the concatenation
 */
Datum
mystr_add(PG_FUNCTION_ARGS)
{
        Mystr *mystr1;
        Mystr *mystr2;
        Mystr *result;

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1)) {
	        PG_RETURN_NULL();
	}
        mystr1 = PG_GETARG_MYSTR(0);
        mystr2 = PG_GETARG_MYSTR(1);
	result = addMystr(mystr1,mystr2);

	PG_RETURN_MYSTR(result);
}

