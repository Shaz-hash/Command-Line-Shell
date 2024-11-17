/**
 * @file utils.h
 * @author 
 * @brief Contains useful macros and utilities to be used by the shell. Define common utility variables/macros/functions which can be used across a lot of file, here. Some useful/not so useful have already been added.
 * @version 0.1
 * @date
 */

#ifndef UTILS_H
#define UTILS_H

// include the logging library
#include "log.h"

// we specify that the strings in our program won't exceed length of 100 characters
#define MAX_STRING_LENGTH 1024

// in order to be consistent, let's just define a macro for copying strings (deep copies)
// some prefer to use strncpy with a buffer you allocate but this is just less hassle
#define COPY(str) (str ? strndup(str, MAX_STRING_LENGTH) : NULL)

// Useful macros for file descriptors to make the code more readable
#define STDIN_FD 0          
#define STDOUT_FD 1    
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1    

#endif // UTILS_H