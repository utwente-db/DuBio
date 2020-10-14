#include "postgres.h"
#include "fmgr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#define SIZE_OF_ORDER 3000
#define SIZE_OF_VARS 3000
#define SIZE_OF_VALUES 9000

typedef struct value{
  int i;
  int val;
  float prob;
  int next;
} value;

typedef struct {
  int id;
  int cardinality;
	char var_name[20];
  int to_first;
} var;

typedef struct {
  //order
  int cur_order; //pointer to the last added element of order[]; if NULL there are no elements added
  int cur_vars; //pointer to the last element of vars[]; if NULL there are no elements added
  int cur_values;  //pointer to the last element of values[]; if no NULL the are no elements added
  int order[SIZE_OF_ORDER];
  var vars[SIZE_OF_VARS];
  value values[SIZE_OF_VALUES];
} dictionary;


void add_var(dictionary * dict, int cardinality, char * var_name);
void add_values(dictionary * dict, int cardinality, int * values, float * probs);
void sort_var(dictionary * dict);
int var_is_in_dictionary(char * var_name, dictionary * dict);
void removeSpaces(char *str);
int check_dict_input(char * input);


void add_var(dictionary * dict, int cardinality, char * var_name){  //adds the variable to vars[]
  int id =0;
  if(dict->cur_vars == -1){
    dict->cur_vars = 0;
    dict->vars[dict->cur_vars].id = 0;
  }
  else {
    id = dict->vars[dict->cur_vars].id;
    dict->cur_vars = dict->cur_vars+1;
    dict->vars[dict->cur_vars].id = id+1;
  }
  dict->vars[dict->cur_vars].cardinality=cardinality;
  strcpy(VARDATA(dict->vars[dict->cur_vars].var_name), VARDATA(var_name));
  if(dict->cur_values == -1)
    dict->vars[dict->cur_vars].to_first = 0;
  else
    dict->vars[dict->cur_vars].to_first = dict->cur_values+1;
}

void add_values(dictionary * dict, int cardinality, int * values, float * probs){   //adds all the values in values[]
  int id =0;
  for (int i=0; i<cardinality; i++){
    if(dict->cur_values == -1){   //if values[] is empty
      dict->cur_values = 0;   //cur_values points to the first element of values[]
      dict->values[dict->cur_values].i = 0;   //set i to 0
    }
    else {
      id = dict->values[dict->cur_values].i;
      dict->cur_values = dict->cur_values+1;    //increments cur_values
      dict->values[dict->cur_values].i= id+1;
    }
    dict->values[dict->cur_values].val = values[i];   //set val
    dict->values[dict->cur_values].prob = probs[i];   //set prob
    if (i<cardinality-1)      //if it is not the last value, next will point to the next one
      dict->values[dict->cur_values].next=dict->cur_values+1;
    else                      //otherwise it will point to -1
      dict->values[dict->cur_values].next = -1;
  }
}
void sort_var(dictionary * dict){   //the function returns the dictionary sorted. (it assumes that the dictionary is already sorted till cur_vars-1)
  int tmp=dict->cur_order;
  if (dict->cur_order==-1)
  {
    dict->cur_order = 0;
    dict->order[dict->cur_order]=dict->cur_vars;
  }
  else{
    while( tmp >= 0){
      if (strcmp(VARDATA(dict->vars[dict->order[tmp]].var_name), VARDATA(dict->vars[dict->cur_vars].var_name))>0 && tmp==0){
        dict->order[tmp+1]=dict->order[tmp];
        dict->order[tmp]=dict->cur_vars;
        tmp--;
      }
      else if (strcmp(VARDATA(dict->vars[dict->order[tmp]].var_name), VARDATA(dict->vars[dict->cur_vars].var_name))>0 && tmp!=0){
        dict->order[tmp+1]=dict->order[tmp];
        tmp--;
      }
      else{
         dict->order[tmp+1]=dict->cur_vars;
         tmp=-1;
      }
    }
    dict->cur_order = dict->cur_order +1;
  }
}

int var_is_in_dictionary(char * var_name, dictionary * dict){     //returns -1 if the variable is not in the dictionary, otherwise the index of the variable in order[]
  for (int i=0; i <= dict->cur_order; i++){
    if (strcmp(VARDATA(dict->vars[dict->order[i]].var_name), VARDATA(var_name)) == 0)
      return i;  //return the index of order[] of that variable
    if (strcmp( VARDATA(dict->vars[dict->order[i]].var_name),VARDATA(var_name)) > 0)
      return -1;
  }
  return -1;
}

void removeSpaces(char *str){     //returns the string without white spaces
    int count = 0;
    for (int i = 0; VARDATA(str)[i]; i++)
        if (VARDATA(str)[i] != ' ')
            VARDATA(str)[count++] = VARDATA(str)[i];
    VARDATA(str)[count] = '\0';
}

int check_dict_input(char * input){   //returns 1 if the input is valid, -1 otherwise
  regex_t re;
  int status;
  const char* pattern;
  removeSpaces(input);
  pattern = "^([a-zA-Z0-9_]+=[0-9]+:[0].[0-9]+;)+$";
  if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) return 0;
  status = regexec(&re, VARDATA(input), 0, NULL, 0);
  regfree(&re);
  if (status != 0) return 0;
  return 1;
}

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(remove_var);
Datum
remove_var(PG_FUNCTION_ARGS){     //removes a variable and all his values from the dictionary
  text  *arg0 = PG_GETARG_TEXT_PP(0);
  int32 arg0_size = VARSIZE_ANY_EXHDR(arg0);
  dictionary* dict;
  char input[arg0_size+1+VARHDRSZ];
  char * var_name;
  char * token;
  int index=-1;
  int token_size;
  bytea* arg =  (bytea*) PG_GETARG_BYTEA_P(1);

  dict = (dictionary*) VARDATA(arg);
  SET_VARSIZE(input, arg0_size+1+VARHDRSZ);
  memcpy(VARDATA(input), VARDATA_ANY(arg0), arg0_size);
  VARDATA(input)[arg0_size]= '\0';

  token = strtok(VARDATA(input), " ");
  token_size= strlen(token);
  var_name = (char*)palloc(token_size+VARHDRSZ+1);
  SET_VARSIZE(var_name, token_size+VARHDRSZ+1);
  memcpy(VARDATA(var_name), token, token_size);
  VARDATA(var_name)[token_size]= '\0';

  if (strtok(NULL, " ")!= NULL)
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
              errmsg("The variable is not in a valid format.")));
  index = var_is_in_dictionary(var_name, dict);
  if (index== -1)
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
              errmsg("The variable %s is not in the dictionary.", VARDATA(var_name))));

  for(int j=index; j<dict->cur_order; j++)
    dict->order[j]=dict->order[j+1];
  dict->cur_order= dict->cur_order-1;
  pfree(var_name);
  PG_RETURN_BYTEA_P(arg);
}

PG_FUNCTION_INFO_V1(add_variable);
Datum
add_variable(PG_FUNCTION_ARGS){     //add a variable with all the values to the dictionary
  text  *arg0 = PG_GETARG_TEXT_PP(0);
  int32 arg0_size = VARSIZE_ANY_EXHDR(arg0);
  dictionary* dict;
  char cpy_input[arg0_size+1];
  char tmp_input[arg0_size+1];
  char * var_name;
  char * token;
  char * token2;
  int * values;
  float * probs;
  int cardinality=0;
  int token2_size=0;
  float sum_prob =0;

  bytea* arg =  (bytea*) PG_GETARG_BYTEA_P(1);
  dict = (dictionary*) VARDATA(arg);
  SET_VARSIZE(tmp_input, arg0_size+1+VARHDRSZ);
  SET_VARSIZE(cpy_input, arg0_size+1+VARHDRSZ);
  memcpy(VARDATA(tmp_input), VARDATA_ANY(arg0), arg0_size);
  memcpy(VARDATA(cpy_input), VARDATA_ANY(arg0), arg0_size);
  VARDATA(tmp_input)[arg0_size]= '\0';
  VARDATA(cpy_input)[arg0_size]= '\0';

  if (dict->cur_vars >= SIZE_OF_VARS)
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("The dictionary is full: too many variables. ")));

  if (!check_dict_input(tmp_input))       //it checks if the sentence is valid
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("Input not valid.")));
  removeSpaces(cpy_input);

  token = strtok(VARDATA(tmp_input), "=");
  while(token){
    cardinality++;
    token = strtok(NULL, "=");
  }
  cardinality--;    //number of values for the variable

  if (dict->cur_values+cardinality >= SIZE_OF_VALUES)
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("The dictionary is full: too many values.")));

  if (cardinality<2)    //min cardinality = 2
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("The variable must have at least 2 values")));

  values= (int *)palloc(cardinality*sizeof(int));
  probs= (float *)palloc(cardinality*sizeof(float));

  token2 =strtok(VARDATA(cpy_input), "=");
  token2_size= strlen(token2);
  var_name = (char*)palloc(token2_size+VARHDRSZ+1);
  SET_VARSIZE(var_name, token2_size+VARHDRSZ+1);
  memcpy(VARDATA(var_name), token2, token2_size);
  VARDATA(var_name)[token2_size]= '\0';

  if (var_is_in_dictionary(var_name, dict) != -1)    //is it already in the dictionary?
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
              errmsg("The variable: %s is already in the dictionary. Enter an other variable.", VARDATA(var_name))));

  for(int i=0; token2!=NULL; i++)   //saves all the values and probabilities
  {
    token2 = strtok(NULL, ":");
    values[i] = atoi(token2);
    token2 = strtok(NULL, ";");
    probs[i] = atof(token2);
    token2 = strtok(NULL, "=");
    if (token2 != NULL && strcmp(token2,VARDATA(var_name))!=0)
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("Input not valid. %s is different from %s.", token2, VARDATA(var_name))));
  }

  for (int i=0; i < cardinality; i++)
    sum_prob = sum_prob + probs[i];

  if (sum_prob != 1)    //the sum of all probabilities must be 1
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
        errmsg("The sum of the probabilities of all the values must be 1, not %f", sum_prob)));

  add_var(dict, cardinality, var_name);
  sort_var(dict);
  add_values(dict, cardinality, values, probs);
  pfree(values);
  pfree(probs);
  pfree(var_name);
  PG_RETURN_BYTEA_P(arg);
}

PG_FUNCTION_INFO_V1(create_dictionary);
Datum
create_dictionary(PG_FUNCTION_ARGS)
{
  dictionary* dict;
  bytea* res = (bytea*)palloc(sizeof(dictionary) + VARHDRSZ);
  SET_VARSIZE(res, sizeof(dictionary) + VARHDRSZ);
  dict = (dictionary*) VARDATA(res);
  dict->cur_order = -1;
  dict->cur_vars = -1;
  dict->cur_values = -1;
  PG_RETURN_BYTEA_P(res);
}
