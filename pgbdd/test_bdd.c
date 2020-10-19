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

#include "bdd.c"

void test_bdd(){
    fprintf(stdout,"Test bdd\n");
    // char* expr = "((x=1 and x=1) or (not y=0 and not zzz=1)) and((xx=2 and x=3)or(not x=2 and not x=3))";
    // char* expr = "((x=1 | x=2) & x=2)";
    char* expr = "x=1 & x=2 | x=3 & x=4 | x=5 & x=6 | x=7 & x=8";
    // (x0 and y1) or (not x0 and not y1) or ((x2 and y2) and y34)
    // char* expr = "(x=1 | x=2)";
    bdd bdd_struct;
    bdd* bdd = bdd_init(&bdd_struct,expr,NULL,NULL,1);
    //
    bdd_start_build(bdd);
    //
    bdd_free(bdd);
}
