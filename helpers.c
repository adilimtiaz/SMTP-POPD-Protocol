//
// Created by Adil Imtiaz on 2018-11-29.
//


#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Taken from: https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
// Note: This function removes all LEADING and TRAILING whitespaces, newlines etc. from a string.
// This function will return a string with len of 0 if a string is only spaces or newline chars
char* trimwhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

// Cuts trailing whitespace and then checks if a newline char is present at the end of the line.
// If anything else is present in str, then it will return false
// Modifies input String
int isLineEndingValid(char *str){
    if(str == NULL){
        // No newline
        return 0;
    }

    // Check if last char of string is \n
    int len = strlen(str);
    // If string is 1 char long and last char is not \n
    // Else lastChar of a string is not \n
    // Then line not valid
    if ((len == 1 && str[0] != '\n') &&
        (str[len - 1] != '\n')) {
        return 0;
    }
    str = trimwhitespace(str);
    // if str is empty it means it only consists of space type chars
    return strlen(str) == 0 ? 1 : 0;
}

// Adapted from: https://cboard.cprogramming.com/c-programming/81565-substr.html
/*  substr("some string", 5, 0, NULL)
    returns "string"
    substr("some string", -5, 3, NULL)
    returns "str"
    substr("some string", 4, 0, "thing")
    returns "something"
 *
 */

char* substr (const char* string, int pos, int len)
{
    char* substring;
    int   i;
    int   length;

    if (string == NULL)
        return NULL;
    length = strlen(string);
    if (pos < 0) {
        pos = length + pos;
        if (pos < 0) pos = 0;
    }
    else if (pos > length) pos = length;
    if (len <= 0) {
        len = length - pos + len;
        if (len < 0) len = length - pos;
    }
    if (pos + len > length) len = length - pos;

    if ((substring = malloc(sizeof(*substring)*(len+1))) == NULL)
        return NULL;
    len += pos;
    for (i = 0; pos != len; i++, pos++)
        substring[i] = string[pos];
    substring[i] = '\0';

    return substring;
}


