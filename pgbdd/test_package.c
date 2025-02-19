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

#include <stdio.h>
#include <stdlib.h>

#include "vector.h"
#include "utils.h"
#include "dictionary.h"
#include "bdd.h"

/*
 *
 */

int main() {
#ifdef BDD_OPTIMIZE
    fprintf(stdout,"# BDD_OPTIMIZE = ON!\n");
#endif
    if ( 0 ) {
        fprintf(stdout,"sizeof(dict_var) = %d\n",(int)sizeof(dict_var));
        fprintf(stdout,"sizeof(dict_val) = %d\n",(int)sizeof(dict_val));
        fprintf(stdout,"sizeof(rva)      = %d\n",(int)sizeof(rva));
        fprintf(stdout,"sizeof(rva_node) = %d\n",(int)sizeof(rva_node));
    }
    //
    if (0) test_vector();
    if (0) test_utils();
    if (0) test_dictionary();
    if (1) test_bdd(); /* INCOMPLETE, SHOULD BE SWITCHED ON AGAIN */
}
