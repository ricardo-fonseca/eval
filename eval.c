/**
 * @file eval.c
 * @author Ricardo Fonseca
 * @brief Toolkit for evaluating student code
 * @version 0.4.3
 * @date 2024-04-18
 * 
 * @copyright Copyright (C) 2024 Ricardo Fonseca
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 */

#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>

#include <math.h>

/**
 * Undefine the replacement macros defined in eval.h so we may call the base
 * functions
 */
#define EVAL_NOWRAP 1
#include "eval.h"

/**
 * @brief Find index of question with given key
 * 
 * @param questions     Question list
 * @param key           Element key
 * @return int          Element index value, -1 if key not found
 */
int question_find( question_t questions[], char key[] ) {
    int idx = -1;
    
    for( int i = 0; i < MAX_QUESTIONS; i++ ) {
        if ( ! strncmp( questions[i].key, "---", 16 ) ) break;
        if ( ! strncmp( questions[i].key, key, 16 ) ) {
            idx = i;
            break;
        }
    }
    return idx;
}

/**
 * @brief Find element text from index
 * 
 * @param questions     Question list
 * @param key       Element key
 * @return char*    Element text, "<not found>" if key not found
 */
char *question_text( question_t questions[], char key[] ) {
    int idx = question_find( questions, key );
    if ( idx < 0 ) {
        return "<not found>";
    } else {
        return questions[idx].text;
    }
}

/**
 * @brief Set element grade
 * 
 * @param questions     Question list
 * @param key       Element key
 * @param grade     New value of grade
 * @return int      Element index or -1 if key not found
 */
int question_setgrade( question_t questions[], char key[], float grade ) {
    int idx = question_find( questions, key );
    if ( idx >= 0 ) {
        questions[idx].grade = grade;
    } else {
        fprintf(stderr,"(*error*) Bad key: %s\n", key );
    }
    return idx;
}

/**
 * @brief Print a detailed list of keys, questions and grades
 * 
 * @param questions     Question list
 * @param msg           Message to print before the list
 * @return int          Number of questions in the list
 */
int question_list( question_t questions[], char* msg ) {
    if ( msg ) printf("\n\033[1m[%s]\033[0m\n", msg);

    printf("\nQuestion list:\n");
    printf("--------------\n");

    double total = 0;

    int i;
    for( i = 0; i < MAX_QUESTIONS; i++ ) {
        if ( ! strcmp( questions[i].key, "---" ) ) break;
        total += questions[i].grade;
        printf("%-7s [%4.2f] - %s\n",questions[i].key, questions[i].grade, questions[i].text );
    }
    printf("\nTotal number of questions: %d\n", i);
    if ( i > 0 ) {
        if ( round(total) == i ) {
            printf("\033[1;32m[✔]\033[0m Total score: %g/%d\n", total, i);
        } else {
            printf("\033[1;31m[✗]\033[0m Total score: %g/%d\n", total, i);
        }
    }
    return i;
}

/**
 * @brief Export question grades
 * 
 * Prints all question keys / grades to screen
 * 
 * @param questions     Question list
 * @param msg   Message to print before / after the grades
 */
void question_export( question_t questions[], char msg[] ) {
    printf("\n%s:grade\n", msg );

    if ( strncmp( questions[0].key, "---", 16 ) ) {
        printf("%s:%4.2f",questions[0].key, questions[0].grade);
        for( int i = 1; i < MAX_QUESTIONS; i++ ) {
            if ( ! strcmp( questions[i].key, "---" ) ) break;
            printf(",%s:%4.2f",questions[i].key, questions[i].grade);
        }
    }

    printf("\n%s:end\n", msg );
}


log_t _success_log;
log_t _error_log;
log_t _data_log;

eval_stats_t _eval_stats;

/**
 * initialize a log_t variable
 * @param log   Log variable to initialize
 */
void initlog( log_t* log ) {
    log -> start = -1;
    log -> end = 0;
}

/**
 * Returns a pointer to a new line in the log and updates internal pointers
 * @param log   Log variable
 * @return      char* to a new line
 */
char *newline( log_t* log ) {
    char *line = NULL;
    if ( log -> end < LOGSIZE ) {
        line = log -> buffer[ log -> end ];
        if ( log -> start < 0 ) log -> start = 0;
        log -> end++;
    } else {
        eval_error( "No more space in logbuffer, aborting" );
        eval_error( "Last message was: \"%s\"", log -> buffer[LOGSIZE-1]);

        // Check if this happened inside an EVAL_CATCH macro
        if ( _eval_env.catch ) {
            siglongjmp( _eval_env.jmp, EVAL_CATCH_LOG_OVERFLOW );
        } else {
            exit(1);
        }
    }
    return line;
}

/**
 * Prints entire log
 * @param log   Log variable
 */
void printlog( log_t* log ) {
    if ( log -> start < 0 || log -> start >= log -> end ) {
        printf("<empty>\n");
    } else {
        for( int i = log -> start, j = 0; i != log -> end; i++, j++ ) {
            printf("%3d - %s\n", j, log -> buffer[i]);
        }
    }
}


/**
 * @brief Looks for the message specified by the format and optional arguments.
 * Returns the index position on success or -1 if not found.
 * 
 * @param log       Log
 * @param format    Format for message
 * @param ...       Optional message values
 * @return int      Position in log or -1 if not found
 */
int findinlog( log_t* log, const char *restrict format, ... ) {
    char msg[LOGLINE];

    va_list ap;
    va_start(ap, format);
    vsnprintf(msg, LOGLINE-1, format, ap);
    va_end(ap);

    int idx = -1;
    for( int i = log->start; i != log->end; i++ ) {
        if ( !strncmp( msg, log->buffer[i], LOGLINE-1) ) {
            idx = i;
            break;
        }
    }
    return idx;
}

/**
 * Removes the head line from the log if it contains the supplied message
 * 
 * @param msg   Messagem to look for
 * @return      0 on success, -1 on error (empty log or msg not found)
 */
int rmheadmsg( log_t* log, const char msg[] ) {
    if ( log -> start < 0 ) return -1;
    if ( strstr( log -> buffer[ log -> start], msg ) ) {
        // Remove the line
        log -> start ++;
        return 0;
    } else {
        return -1;
    }
}

/**
 * Returns the string at the head of the log or "empty" if empty
 * 
 * @param log   Log
 * @return      Pointer to string
 */
const char* loghead( log_t* log ) {
    static const char empty[] = "<empty>";
    if ( log -> start < 0 ) return empty;
    else return log -> buffer[log -> start];
}


/**
 * @brief Adds line into specified log
 * 
 * @param log       Pointer to log variable
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Number of characters written
 */
int eval_log( log_t* log, const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(newline( log ), LOGLINE, format, ap);
    va_end(ap);
    return n;
}

/**
 * @brief Adds line into datalog
 * 
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Number of characters written
 */
int datalog(const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(newline( &_data_log ), LOGLINE, format, ap);
    va_end(ap);
    return n;
}

/**
 * @brief Adds line into datalog
 * 
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Number of characters written
 */
int errorlog(const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(newline( &_error_log ), LOGLINE, format, ap);
    va_end(ap);
    return n;
}

/**
 * @brief Adds line into successlog
 * 
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Number of characters written
 */
int successlog(const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(newline( &_success_log ), LOGLINE, format, ap);
    va_end(ap);
    return n;
}

/**
 * @brief Checks if the supplied message (specified by format and optionally additional values)
 * is at the head of the success log.
 * 
 * If true, removes head line and returns 0, otherwise issues an error message.
 * 
 * @param format    Format for message
 * @param ...       (optional) additional values
 * @return int      1 on success, 0 if message not found at head
 */
int eval_check_successlog( const char *restrict format, ...) {
    char msg[LOGLINE];

    va_list ap;
    va_start(ap, format);
    vsnprintf(msg, LOGLINE-1, format, ap);
    va_end(ap);

    const char *log_msg = loghead(&_success_log);
    if ( rmheadmsg( &_success_log, msg ) ) {
        eval_error( "Invalid success log message, expected '%s', got '%s'",
            msg, log_msg );
        return 0;
    } else { 
        eval_success( "Success log ok: '%s'", log_msg );
        return 1;
    }
}

/**
 * @brief Checks if the supplied message (specified by format and optionally additional values)
 * is at the head of the error log.
 * 
 * If true, removes head line and returns 0, otherwise issues an error message.
 * 
 * @param format    Format for message
 * @param ...       (optional) additional values
 * @return int      1 on success, 0 if message not found at head
 */
int eval_check_errorlog( const char *restrict format, ...) {
    char msg[LOGLINE];

    va_list ap;
    va_start(ap, format);
    vsnprintf(msg, LOGLINE-1, format, ap);
    va_end(ap);

    const char *err_msg = loghead(&_error_log);
    if ( rmheadmsg( &_error_log, msg ) ) {
        eval_error( "Invalid error log message, expected '%s', got '%s'",
            msg, err_msg );
        return 0;
    } else { 
        eval_success( "Error log ok: '%s'", err_msg );
        return 1;
    }
}

/**
 * Clears both success and error logs
 */
void eval_clear_logs( void ) {
    initlog( &_success_log );
    initlog( &_error_log );
    initlog( &_data_log );
}

/**
 * Clears both success and error logs, and prints remaning messages if any
 */
void eval_close_logs( char msg[] ) {
    if ( _success_log.start >= 0 && _success_log.start < _success_log.end ) {
        eval_info( "%s Remaining messages on success log", msg );
        for( int i = _success_log.start; i != _success_log.end; i++ ) {
            printf("%3d - %s\n", i, _success_log.buffer[i]);
        }
    } 

    if ( _error_log.start >= 0 && _error_log.start < _error_log.end ) {
        eval_info( "%s Remaining messages on error log", msg );
        for( int i = _error_log.start; i != _error_log.end; i++ ) {
            printf("%3d - %s\n", i, _error_log.buffer[i]);
        }
    } 
    
    eval_clear_logs( );
}

/**
 * @brief Checks errors at section completion
 * 
 * @param msg   Message to print
 * @return int  Total of errors found
 */
int eval_complete( char msg[] ) {
    if ( _eval_stats.error > 0 ) {
        printf("\033[1;31m[✗]\033[0m %s completed with %d error(s).\n", msg, _eval_stats.error );
    } else {
        printf("\033[1;32m[✔]\033[0m %s completed with no errors.\n", msg );
    }
    printf("\n");
    return _eval_stats.error;
}

/**
 * @brief Prints error message and updates error counter
 * 
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Current number of errors
 */
int eval_error(const char *restrict format, ...) {
    va_list ap;
    size_t size = 0;
    char *tmp = NULL;

    va_start(ap, format);
    int n = vsnprintf(tmp, size, format, ap);
    va_end(ap);

    // Allocate temporary buffer
    size = (size_t) n + 1;
    tmp = malloc( size );

    // Print to buffer
    va_start(ap, format);
    vsnprintf(tmp, size, format, ap);
    va_end(ap);

    printf("\033[1;31m[✗]\033[0m %s\n", tmp );

    free(tmp);

    _eval_stats.error++;

    return _eval_stats.error;
}

/**
 * @brief Prints info message and updates the info counter
 * 
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Current number of errors
 */
int eval_info(const char *restrict format, ...) {
    va_list ap;
    size_t size = 0;
    char *tmp = NULL;

    va_start(ap, format);
    int n = vsnprintf(tmp, size, format, ap);
    va_end(ap);

    // Allocate temporary buffer
    size = (size_t) n + 1;
    tmp = malloc( size );

    // Print to buffer
    va_start(ap, format);
    vsnprintf(tmp, size, format, ap);
    va_end(ap);

    printf("\033[1;34m[ℹ︎]\033[0m %s\n", tmp );

    free(tmp);

    _eval_stats.info++;

    return _eval_stats.info;
}

/**
 * @brief Prints success message and updates info counter
 * 
 * @param format    Format modifier
 * @param ...       Values
 * @return int      Current number of errors
 */
int eval_success(const char *restrict format, ...) {
    va_list ap;
    size_t size = 0;
    char *tmp = NULL;

    va_start(ap, format);
    int n = vsnprintf(tmp, size, format, ap);
    va_end(ap);

    // Allocate temporary buffer
    size = (size_t) n + 1;
    tmp = malloc( size );

    // Print to buffer
    va_start(ap, format);
    vsnprintf(tmp, size, format, ap);
    va_end(ap);

    printf("\033[1;32m[✔]\033[0m %s\n", tmp );

    free(tmp);

    _eval_stats.info++;

    return _eval_stats.info;
}

/**
 * @brief Variable holding stdin and stdout descriptors (previous and current)
 * 
 */
_eval_stdio_t _eval_stdio;

/**
 * @brief Redirects stdin and stdout to files
 *
 * Requires data in _eval_stdio
 * 
 * @param stdin     File name for stdin. If set to NULL no redirection takes place
 * @param stdout    File name for stdout. If set to NULL no redirection takes place
 * @return int      0 on success
 */
int _eval_io_redirect( const char* _fstdin, const char* _fstdout ) {

    if ( _fstdin ) {
        if ( (_eval_stdio.stdin_old = dup( STDIN_FILENO ))< 0 ) {
            eval_error("Unable to duplicate STDIN_FILENO");
            return -1;
        }

        if ( close( STDIN_FILENO ) < 0 ) {
            eval_error("Unable to close STDIN_FILENO");
            return -1;
        }

        if ( (_eval_stdio.stdin = open( _fstdin, O_RDONLY )) < 0 ) {
            eval_error("Unable to open file %s as read-only", _fstdin);
            return -1;
        }
        if ( (dup2( _eval_stdio.stdin, STDIN_FILENO )) < 0 ) {
            eval_error("Unable to associate file %s with stdin", _fstdin);
            return -1;
        };
    } else {
        _eval_stdio.stdin_old = -1;
        _eval_stdio.stdin = -1;
    }

    if ( _fstdout ) {
        if ( (_eval_stdio.stdout_old = dup( STDOUT_FILENO )) < 0 ) {
            fprintf(stderr, "Unable to duplicate STDOUT_FILENO\n");
            eval_error("Unable to duplicate STDOUT_FILENO");
            return -1;
        }

        if ( close( STDOUT_FILENO )  < 0 ) {
            eval_error("Unable to close STDOUT_FILENO");
            return -1;
        }

        // Remove file if it exists
        unlink( _fstdout );

        if ( (_eval_stdio.stdout = open( _fstdout, O_CREAT | O_WRONLY )) < 0 ) {
            fprintf(stderr, "Unable to open file %s as write-only\n", _fstdout);
            eval_error("Unable to open file %s as write-only", _fstdout);
            return -1;
        }
        if ( (dup2( _eval_stdio.stdout, STDOUT_FILENO )) < 0 ) {
            fprintf(stderr, "Unable to associate file %s with stdout\n", _fstdout);
            eval_error("Unable to associate file %s with stdout", _fstdout);
            return -1;
        }
    } else {
        _eval_stdio.stdout_old = -1;
        _eval_stdio.stdout = -1;
    }

    return 0;
}

/**
 * @brief Restores stdin and stdout to console
 *
 * Requires data in _eval_stdio
 *
 * @return int      0 on success
 */
int _eval_io_restore( void ) {

    if ( _eval_stdio.stdin != -1 ) {

        if ( close( _eval_stdio.stdin ) < 0 ) {
            fprintf(stderr, "Unable to close stdin file\n" );
            return 1;
        }
        if ( dup2( _eval_stdio.stdin_old, STDIN_FILENO ) < 0 ) {
            fprintf(stderr, "Unable to reassociate STDIN with console\n" );
            return 1;
        }

        if ( close( _eval_stdio.stdin_old ) < 0 ) {
            fprintf(stderr, "Unable to close stdin_old file\n" );
            return 1;
        }

    }

    if ( _eval_stdio.stdout != -1 ) {

        // Flush any remaining data to disk
        // If this is not done then this data will be sent to the next STDOUT device
        fflush( stdout );

        if ( close( _eval_stdio.stdout ) < 0 ) {
            fprintf(stderr, "Unable to close stdout file\n" );
            return 1;
        }

        if ( dup2( _eval_stdio.stdout_old, STDOUT_FILENO ) < 0 ) {
            fprintf(stderr, "Unable to reassociate STDOUT with console\n" );
            return 1;
        }

        if ( close( _eval_stdio.stdout_old ) < 0 ) {
            fprintf(stderr, "Unable to close stdin_old file\n" );
            return 1;
        }

    }

    return 0;
}

/**
 * @brief Initialize file monitor variable
 * 
 * This value will be used to check if the test routine left any files open
 */
void _eval_init_filemon( void ) {
    // Get the file descriptor value of a new file
    // All files opened from test code should have a descriptor >= than this
    _eval_env.filemon = dup(STDIN_FILENO);
    close(_eval_env.filemon);
}

/**
 * @brief Closes any files open (besides STDIN, STDOUT and STDERR)
 * 
 * Requires values in _eval_env.filemon
 */
void _eval_close_filemon( void ) {

    // Get maximum number of open file descriptors
    const int maxfd=sysconf(_SC_OPEN_MAX);

    // Go through all possible file descriptors starting from .filemon
    int nfiles = 0;
    for( int fd = _eval_env.filemon; fd < maxfd; fd++ ) {
        if ( close(fd) ) {
            // If file was not open, errno is set to EBADF
            if ( errno != EBADF ) {
                // File is opened but close failed for another reason
                nfiles++;
                perror("_eval_close_filemon: Unable to close file");
            }
        } else {
            // File closed successfully
            nfiles++;
        }
    }

    if ( nfiles > 0 ) {
        if ( nfiles == 1 ) {
            eval_error("1 file was not closed" );
        } else {
            eval_error("%d files were not closed", nfiles );
        }
    }

}

/**
 * @brief Resets the _eval_stats variable
 * 
 */
void eval_reset_stats( void ) {
    _eval_stats.error = 0;
    _eval_stats.info = 0;
}

/**
 * @brief Configuration variable for the eval_checkptr() function
 * 
 */
struct {
    int stat;
    int sig;
    jmp_buf jmp;
    char buffer;
}_eval_checkptr_data;

/**
 * @brief Signal handler for the eval_checkptr function
 * Requires data in the _eval_checkptr_data variable
 * 
 * @param sig 
 * @param info 
 * @param ucontext 
 */
void _eval_checkptr_sighndlr( int sig, siginfo_t *info, void *ucontext) {
    _eval_checkptr_data.stat = 1;
    _eval_checkptr_data.sig = sig;
    siglongjmp( _eval_checkptr_data.jmp, 1 );
}

/**
 * Checks if a pointer is valid by reading/writing 1 byte from/to address
 * @param ptr   Pointer to be evaluated
 * @return      0 is pointer is valid
 *              1 NULL pointer
 *              2 (-1) pointer
 *              3 Segmentation fault when accessing pointer
 *              4 Bus Error when accessing pointer
 *              5 Invalid signal caught (should never happen)
 */
int eval_checkptr( void* ptr ) {

    if ( ptr == NULL ) {
        eval_error("NULL pointer");
        return 1;
    }

    if ( ptr == (void *) -1 ) {
        eval_error("Invalid pointer %p", ptr );
        return 2;
    }

    // Catch SIGSEGV and SIGBUS
    struct sigaction act = {
        .sa_flags = SA_SIGINFO | SA_RESTART,
        .sa_sigaction = _eval_checkptr_sighndlr
    };
    struct sigaction tmp[2];

    if ( sigaction( SIGSEGV, &act, &tmp[0] ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to set SIGSEGV handler");
        exit(1);
    };

    if ( sigaction( SIGBUS, &act, &tmp[1] ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to set SIGBUS handler");
        exit(1);
    };

    // Reset _eval_checkptr_data
    _eval_checkptr_data.stat = 0;
    _eval_checkptr_data.sig = -1;

    // Access memory at position ptr, invalid pointers will throw a signal
    int stat = sigsetjmp( _eval_checkptr_data.jmp, 1 );
    if ( !stat ) {
        char *p = (char *) ptr;
        _eval_checkptr_data.buffer = *p;
        *p = _eval_checkptr_data.buffer;
    }

    // Restore previous signal handlers
    if ( sigaction( SIGSEGV, &tmp[0], NULL ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to reset SIGSEGV handler");
        exit(1);
    };

    if ( sigaction( SIGBUS, &tmp[0], NULL ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to reset SIGBUS handler");
        exit(1);
    };

    if ( _eval_checkptr_data.stat ) {
        switch( _eval_checkptr_data.sig ) {
        case(SIGSEGV):
            eval_error("(checkptr) Accessing %p caused Segmentation Fault", ptr);
            return 3;
        case(SIGBUS):
            eval_error("(checkptr) Accessing %p caused Bus Error", ptr);
            return 4;
        default:
            eval_error("(checkptr) Accessing %p caused unknonown signal", ptr);
            return 5;
        }
    }

    return 0;
}

/**
 * Checks if a pointer is valid by reading 1 byte from address
 * @param ptr   Pointer to be evaluated
 * @return      0 is pointer is valid
 *              1 NULL pointer
 *              2 (-1) pointer
 *              3 Segmentation fault when accessing pointer
 *              4 Bus Error when accessing pointer
 *              5 Invalid signal caught (should never happen)
 */
int eval_checkconstptr( const void * ptr ) {

    if ( ptr == NULL ) {
        eval_error("NULL pointer");
        return 1;
    }

    if ( ptr == (void *) -1 ) {
        eval_error("Invalid pointer %p", ptr );
        return 2;
    }

    // Catch SIGSEGV and SIGBUS
    struct sigaction act = {
        .sa_flags = SA_SIGINFO | SA_RESTART,
        .sa_sigaction = _eval_checkptr_sighndlr
    };
    struct sigaction tmp[2];

    if ( sigaction( SIGSEGV, &act, &tmp[0] ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to set SIGSEGV handler");
        exit(1);
    };

    if ( sigaction( SIGBUS, &act, &tmp[1] ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to set SIGBUS handler");
        exit(1);
    };

    // Reset _eval_checkptr_data
    _eval_checkptr_data.stat = 0;
    _eval_checkptr_data.sig = -1;

    // Access memory at position ptr, invalid pointers will throw a signal
    int stat = sigsetjmp( _eval_checkptr_data.jmp, 1 );
    if ( !stat ) {
        char *p = (char *) ptr;
        _eval_checkptr_data.buffer = *p;
    }

    // Restore previous signal handlers
    if ( sigaction( SIGSEGV, &tmp[0], NULL ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to reset SIGSEGV handler");
        exit(1);
    };

    if ( sigaction( SIGBUS, &tmp[0], NULL ) < 0 ) {
        perror("eval_checkptr: (*critical*) Unable to reset SIGBUS handler");
        exit(1);
    };

    if ( _eval_checkptr_data.stat ) {
        switch( _eval_checkptr_data.sig ) {
        case(SIGSEGV):
            eval_error("(checkptr) Accessing %p caused Segmentation Fault", ptr);
            return 3;
        case(SIGBUS):
            eval_error("(checkptr) Accessing %p caused Bus Error", ptr);
            return 4;
        default:
            eval_error("(checkptr) Accessing %p caused unknonown signal", ptr);
            return 5;
        }
    }

    return 0;
}

/**
 * @brief Reads a line from file f and compares it with the string s2
 * 
 * @param f 
 * @param s2 
 * @param n         Max. size for the comparison
 * @return int      Returns
 */
int eval_fstrncmp( FILE *f, const char *s2, const size_t n ) {
    int ret;
    if (f) {
        char buffer[ n + 1 ];
        fgets( buffer, n, f );

        size_t size = strlen( s2 );
        if ( n < size ) size = n;

        // printf( "eval_fstrncmp: %s, %s, %ld ", buffer, s2, n );

        ret = strncmp( buffer, s2, size );
    } else {
        ret = -2;
    }
    return ret;
}


/**
 * @brief Creates a "locked" file, i.e. a file with 0000 permissions
 *
 * Note that this file can still be removed using unlink() or remove()
 * 
 * @param fname     Name of the file to create
 *
 * @return int      0 on success, -1 on error
 */
int create_lockfile( char *fname ) {
       
    // Open file and write magic bytes
    FILE *f = fopen( fname, "w" );
    if ( f ==  NULL ) {
        perror("create_lockfile: Unable to create lock file");
        return -1;
    } else {
        char magic[] = "LOCK";
        if ( fwrite( magic, 1, 4, f ) != 4 ) {
            perror("create_lockfile: Unable to write magic bytes to lock file");
            return -1;
        };

        if ( fclose(f) ) {
            perror("create_lockfile: Unable to close lock file");
            return -1;
        }

        // Remove all permissions
        if ( chmod( fname, 0000 ) ) {
            perror("create_lockfile: Unable to set lock file permissions");
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Removes a "locked" file created by create_lockfile()
 * 
 * User must be file owner.
 * 
 * @param fname     Name of the file to create 
 *
 * @return int      0 on success, -1 on error
 */
int remove_lockfile( char *fname ) {
    struct stat st;
    if ( stat( fname, &st ) < 0 ) {
        perror("remove_lockfile: File no longer exists" );
        return -1;
    }

    // Unlink does not check write permission
    if ( unlink( fname ) ) {
        perror("remove_lockfile: Unable to remove lock file");
        return -1;
    };

    return 0;
}


/**
 * Global variable for use by the EVAL_CATCH* macros
 */
_eval_env_type _eval_env = {0};

/**
 * Global variable for use by the eval_termination() function
 */
char _eval_term_str[256] = {0};

/**
 * @brief Pointer to string describing the test code termination
 *
 * Requires values in _eval_env. Note that the function always returns a pointer
 * to the same location, the global _eval_term_str variable is modified with the
 * updated termination message.
 * 
 * @return const char*      Test code termination description
 */
const char* eval_termination( void ) {
    char *signame;
    
    switch( _eval_env.stat ) {
        case(0):
            snprintf(_eval_term_str, 255, "code returned normally" );
            break;
        case(EVAL_CATCH_EXIT):
            snprintf(_eval_term_str, 255, "exit(%d) called", _eval_exit_data.status );
            break;
        case(EVAL_CATCH_ABORT):
            snprintf(_eval_term_str, 255, "abort() called" );
            break;
        case(EVAL_CATCH_BLOCKED):
            snprintf(_eval_term_str, 255, "blocked function called" );
            break;
        case(EVAL_CATCH_SIGNAL):
            signame = strsignal(_eval_env.signal);
            if ( signame ) {
                snprintf(_eval_term_str, 255, "signal %s caught", signame );
            } else {
                snprintf(_eval_term_str, 255, "Unknown signal %d caught", _eval_env.signal );
            }
            break;
        case(EVAL_CATCH_LOG_OVERFLOW):
            snprintf(_eval_term_str, 255, "log buffer full" );
            break;
        default:
            snprintf(_eval_term_str, 255, "abnormal termination");
    }
    return _eval_term_str;
}

/**
 * @brief Signal handler for the EVAL_CATCH* macros
 *
 * Requires data in the _eval_env variable
 * 
 * @param sig           Signal caught
 * @param info          Pointer to object explaining the reason why the signal
 *                      was generated
 * @param ucontext      Additional information from the process sending the 
 *                      signal
 */
void _eval_sighandler( int sig, siginfo_t *info, void *ucontext) {

    char *name;

    switch( sig ) {
    case( SIGSEGV ):
        eval_error("Segmentation fault (SIGSEGV)");
        break;
    case( SIGBUS ):
        eval_error("Bus error (SIGBUS)");
        break;
    case( SIGFPE ):
        eval_error("Floating point exception / division by 0 (SIGFPE)");
        break;
    case( SIGILL ):
        eval_error("Illegal instruction (SIGILL)");
        break;
    case( SIGPROF ):
        eval_error("Timeout (SIGPROF)");
        break;
    default:
        name = strsignal(sig);
        if ( name ) {
            eval_error("Unexepected signal %s (%d) caught!", name, sig);
        } else {
            eval_error("Unexepected signal %d caught!", sig);
        }
    }

    _eval_env.signal = sig;
    siglongjmp(_eval_env.jmp, EVAL_CATCH_SIGNAL);
}

/**
 * @brief Arms signals for the EVAL_CATCH* macros and sets timeout alarm
 * 
 * Requires data in the _eval_env variable
 *
 * Specifically, SIGSEGV, SIGBUS, SIGFPE and SIGILL will be caught.
 *
 * If _eval_env.timeout > 0 then a timout alarm for _eval_env.timeout seconds
 * will be set using the SIGPROF signal. Note that if _eval_env.timeout <= 0
 * SIGPROF will not be armed.
 *
 * The routine will also store any previous signal handlers in 
 * _eval_env.sigactions[*] so that these may be restored later
 */
void _eval_arm_signals( void ) {

    struct sigaction act;

    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = _eval_sighandler;

    if ( sigaction( SIGSEGV, &act, &_eval_env.sigactions[0] ) < 0) {
        perror("_eval_arm_signals: (*critical*) Unable to set signal handler for SIGSEGV");
        exit(1);
    }

    if ( sigaction( SIGBUS, &act, &_eval_env.sigactions[1] ) < 0) {
        perror("_eval_arm_signals: (*critical*) Unable to set signal handler for SIGBUS");
        exit(1);
    }

    if ( sigaction( SIGFPE, &act, &_eval_env.sigactions[2] ) < 0) {
        perror("_eval_arm_signals: (*critical*) Unable to set signal handler for SIGFPE");
        exit(1);
    }

    if ( sigaction( SIGILL, &act, &_eval_env.sigactions[3] ) < 0) {
        perror("_eval_arm_signals: (*critical*) Unable to set signal handler for SIGILL");
        exit(1);
    }

    // Timeouts
    if( _eval_env.timeout > 0 ) {
        if ( sigaction( SIGPROF, &act, &_eval_env.sigactions[4] ) < 0) {
            perror("_eval_arm_signals: (*critical*) Unable to set signal handler for SIGPROF");
            exit(1);
        }

        struct itimerval value;
        value.it_value.tv_sec = floor( _eval_env.timeout );
        value.it_value.tv_usec = floor( (_eval_env.timeout - value.it_value.tv_sec) * 1.e6 ) ;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_usec = 0;

        if ( setitimer( ITIMER_PROF, &value, NULL ) < 0 ) {
            perror("_eval_arm_signals: (*critical*) Unable to set timeout itimer");
            exit(1);
        }
    }

    // Reset _eval_env.signal
    _eval_env.signal = -1;
}

/**
 * @brief Restores previous signal handlers
 * 
 * Requires data in the _eval_env variable
 * 
 */
void _eval_disarm_signals( void ) {

    if( _eval_env.timeout > 0 ) {

        struct itimerval value;
        value.it_value.tv_sec = 0;
        value.it_value.tv_usec = 0;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_usec = 0;

        if ( setitimer( ITIMER_PROF, &value, NULL ) < 0 ) {
            perror("_eval_disarm_signals: (*critical*) Unable to reset timeout itimer");
            exit(1);
        }

        if ( sigaction( SIGPROF, &_eval_env.sigactions[4], NULL ) < 0) {
            perror("_eval_disarm_signals: (*critical*) Unable to reset signal handler for SIGPROF");
            exit(1);
        }
    }

    if ( sigaction( SIGSEGV, &_eval_env.sigactions[0], NULL ) < 0) {
        perror("_eval_disarm_signals: (*critical*) Unable to reset signal handler for SIGSEGV");
        exit(1);
    }

    if ( sigaction( SIGBUS, &_eval_env.sigactions[1], NULL ) < 0) {
        perror("_eval_disarm_signals: (*critical*) Unable to reset signal handler for SIGBUS");
        exit(1);
    }

    if ( sigaction( SIGFPE, &_eval_env.sigactions[2], NULL ) < 0) {
        perror("_eval_disarm_signals: (*critical*) Unable to reset signal handler for SIGFPE");
        exit(1);
    }

    if ( sigaction( SIGILL, &_eval_env.sigactions[3], NULL ) < 0) {
        perror("_eval_disarm_signals: (*critical*) Unable to reset signal handler for SIGILL");
        exit(1);
    }

}

/******************************************************************************
 * Function wrappers
 *****************************************************************************/

/**
 * @brief Global _eval_exit_data variable for the exit() function
 * 
 */
EVAL_VAR(exit);

/**
 * @brief Evaluate implementation calling of exit function
 *
 * Requires data in global _eval_exit_data and _eval_env
 *
 * Note that in this case the wrapped function will never be called. Instead,
 * execution will be returned to the EVAL_CATCH macro,setting the appropriate
 * _eval_env and _eval_exit_data values.
 *
 * Behavior:
 *   .action = 1    Issue info message and return to EVAL_CATCH macro
 *   default        return to EVAL_CATCH macro
 * 
 * @param status    Exit status
 */
void _eval_exit( int status ) {
    _eval_exit_data.status = status;
    if ( _eval_exit_data.action == ACTION_WARN )
         eval_info("exit(%d) caught!", status );
    
    siglongjmp(_eval_env.jmp, EVAL_CATCH_EXIT);
}

/**
 * @brief Global _eval_abort_data variable for the abort() function
 * 
 */
EVAL_VAR(abort);

/**
 * @brief Evaluate implementation calling of abort function
 *
 * Requires data in global _eval_abort_data and _eval_env
 *
 * Note that in this case the wrapped function will never be called. Instead,
 * execution will be returned to the EVAL_CATCH macro,setting the appropriate
 * _eval_env and _eval_abort_data values.
 *
 * Behavior:
 *   .action = 1    Issue info message and return to EVAL_CATCH macro
 *   default        return to EVAL_CATCH macro
 * 
 * @param status    Exit status
 */
void _eval_abort( void ) {
    _eval_abort_data.status = 1;
    if ( _eval_abort_data.action == ACTION_WARN )
         eval_info("abort() caught!" );
    
    siglongjmp(_eval_env.jmp, EVAL_CATCH_ABORT);
}

/**
 * @brief Global _eval_sleep_data variable for the sleep() function
 * 
 */
EVAL_VAR(sleep);

/**
 * @brief Evaluate implementation calling of sleep function
 * Requires data in global _eval_sleep_data
 * 
 * .action behavior:
 *   ACTION_ERROR       (interrupted by signal) Return 1
 *   ACTION_LOG         (log) Log function call and proceed with ACTION_SUCCESS
 *   ACTION_SUCCESS     (success) Return 0
 *   ACTION_DEFAULT     call sleep(seconds)
 * 
 * @param seconds   Number of seconds to sleep
 * @return          Evalation result or result of sleep operation
 */
unsigned int _eval_sleep(unsigned int seconds) {
    _eval_sleep_data.status ++;
    _eval_sleep_data.seconds = seconds;
    switch( _eval_sleep_data.action ) {
    case(ACTION_ERROR): // Interrupted by signal
        _eval_sleep_data.ret = 1;
        break;
    case(ACTION_LOG):
        datalog("sleep,%d", seconds );
    case(ACTION_SUCCESS): // success (finished sleep)
        _eval_sleep_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("sleep() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;
    default:
        _eval_sleep_data.ret = sleep( seconds );
    }
    return _eval_sleep_data.ret;
}

/**
 * @brief Global _eval_fork_data variable for the fork() function
 * 
 */
EVAL_VAR(fork);

/**
 * @brief Evaluate implementation calling of fork function
 * Requires data in global _eval_fork_data
 * 
 * .action behavior:
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`
 * + `ACTION_SUCCESS` - (success) Return value previously set in `_eval_fork_data.ret`.
 *                      If the value was less than 0 it is set to 0. If we want the 
 *                      command to behave as if it returns in the parent process, we 
 *                      should set this to a value `>= 1`.
 * + `ACTION_DEFAULT` - Capture parameters and call `fork()`
 * 
 * @return          Evalation result or result of fork operation
 */
pid_t _eval_fork(void) {
    _eval_fork_data.status ++;
    switch( _eval_fork_data.action ) {
    case(ACTION_ERROR): // error
        _eval_fork_data.ret = -1;
        break;
    case(ACTION_LOG):
        datalog("fork");
    case(ACTION_SUCCESS): // success
        // Returns the value present in _eval_fork_data.ret
        if ( _eval_fork_data.ret < 0 ) _eval_fork_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("fork() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;
    default:
        _eval_fork_data.ret = fork( );
    }
    return _eval_fork_data.ret;
}

/**
 * @brief Global _eval_wait_data variable for the wait() function
 * 
 */
EVAL_VAR(wait);

/**
 * Evaluate implementation calling of wait function
 * Requires data in global _eval_wait_data
 * 
 * .action behavior:
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`
 * + `ACTION_SUCCESS` - (success) Return value previously set in `_eval_fork_data.ret`. 
 *                      If the value was less than 0 it is set to 0.
 * + `ACTION_DEFAULT` - Capture parameters and call `wait(stat_loc)`
 *
 * @return          Evalation result or result of wait operation
 */
pid_t _eval_wait(int *stat_loc) {

    _eval_wait_data.status ++;

    int err = 0;
    if ( stat_loc != NULL ) {
        if ( eval_checkptr( stat_loc ) != 0 ) {
            eval_error("wait() called with invalid pointer (stat_loc)\n");
            err++;
        }
    }

    switch( _eval_wait_data.action ) {

    case(ACTION_ERROR): // error
        _eval_wait_data.ret = -1;
        errno = EINVAL;
        break;

    case(ACTION_LOG):
        datalog("wait,%p", stat_loc);

    case(ACTION_SUCCESS): // success
        if ( _eval_wait_data.ret < 0 ) _eval_wait_data.ret = 0;
        if ( !err && stat_loc ) {
            if ( _eval_wait_data.stat_loc ) *stat_loc = *_eval_wait_data.stat_loc;
            else *stat_loc = 0;
        }
        break;

    case(ACTION_BLOCK):
        eval_error("wait() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_wait_data.ret = wait( stat_loc );
        } else {
            _eval_wait_data.ret = -1;
        }
    }

    _eval_wait_data.stat_loc = stat_loc;
    return _eval_wait_data.ret;
}

/**
 * @brief Global _eval_waipid_data variable for the waitpid() function
 * 
 */
EVAL_VAR(waitpid);

/**
 * Evaluate implementation calling of waitpid function
 * Requires data in global _eval_waitpid_data
 * 
 * Behavior:
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`
 * + `ACTION_SUCCESS` - (success) Return value previously set in `_eval_fork_data.ret`.
 *                      If the value was less than 0 it is set to 0. If `stat_loc` is
 *                      not NULL then `*stat_loc` will be set to `*_eval_wait_data.stat_loc`,
 *                      if this is not NULL, or 0 otherwise
 * + `ACTION_DEFAULT` - Capture parameters and call `waitpid(pid,stat_loc,options)`
 *
 * @return          Evalation result or result of wait operation
 */
pid_t _eval_waitpid(pid_t pid, int *stat_loc, int options) {

    int err = 0;
    if ( stat_loc != NULL ) {
        if ( eval_checkptr( stat_loc ) != 0 ) {
            eval_error("waitpid() called with invalid pointer (stat_loc)\n");
            err ++;
        }
    }

    _eval_waitpid_data.status ++;


    switch( _eval_waitpid_data.action ) {

    case(ACTION_ERROR):
        _eval_waitpid_data.stat_loc = stat_loc;
        _eval_waitpid_data.ret = -1;
        errno = EINVAL;
        break;

    case(ACTION_LOG):
        datalog("waitpid,%d,%p,%d", pid,stat_loc,options);

    case(ACTION_SUCCESS):
        if ( pid <= 0 ) {
            _eval_waitpid_data.ret = _eval_waitpid_data.pid;
        } else {
            _eval_waitpid_data.ret = pid;
        }
        if ( !err && stat_loc ) {
            if ( _eval_waitpid_data.stat_loc ) *stat_loc = *_eval_waitpid_data.stat_loc;
            else *stat_loc = 0;
        }
        break;

    case(ACTION_BLOCK):
        eval_error("waitpid() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_waitpid_data.ret = waitpid( pid, stat_loc, options );
        } else {
            _eval_waitpid_data.ret = -1;
        }
    }

    _eval_waitpid_data.pid = pid;
    _eval_waitpid_data.options = options;
    _eval_waitpid_data.stat_loc = stat_loc;

    return _eval_waitpid_data.ret;
}

/**
 * @brief Global _eval_kill_data variable for the kill() function
 * 
 */
EVAL_VAR(kill);

/**
 * Evaluate implementation calling of kill function
 * Requires data in global _eval_kill_data
 * 
 * Behavior:
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `kill(pid,sig)`
 * + `ACTION_PROTECT` - Same as default `ACTION_DEFAULT` but prevent routine from
 *                      signaling self, parent, process group and every process 
 *                      belonging to process owner
 *
 * @param pid   Process(es) to send signal to
 * @param sig   Signal to be sent
 * @return      error status
 */
int _eval_kill(pid_t pid, int sig) {
    _eval_kill_data.status ++;
    _eval_kill_data.pid = pid;
    _eval_kill_data.sig = sig;

    int err;

    switch( _eval_kill_data.action ) {
    case(ACTION_ERROR): // error
        _eval_kill_data.ret = -1;
        errno = EINVAL;
        break;

    case(ACTION_LOG): // log
        datalog("kill,%d,%d", pid, sig );

    case(ACTION_SUCCESS): // success
        _eval_kill_data.ret = 0;
        break;

    case(ACTION_BLOCK):
        eval_error("kill() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    case(ACTION_PROTECT): // Catch bad pid values
        err = 0;
        if ( getpid() == pid ) {
            eval_error("(kill) prevented sending signal to self");
            err = 1;
        }

        if ( getppid() == pid ) {
            eval_error("(kill) prevented sending signal to parent");
            err = 1;
        }

        if ( 0 == pid ) {
            eval_error("(kill) prevented sending signal to every process in the process group");
            err = 1;
        }

        if ( -1 == pid ) {
            eval_error("(kill) prevented sending signal to to every process belonging to process owner");
            err = 1;
        }
        
        if ( err ) {
            _eval_kill_data.ret = 0;
            break;
        }

    default:    // send signal
        _eval_kill_data.ret = kill( pid, sig );
    }
    return _eval_kill_data.ret;
}

/**
 * @brief Global _eval_raise_data variable for the raise() function
 * 
 */
EVAL_VAR(raise);

/**
 * @brief Evaluate implementation calling of raise function
 * Requires data in global _eval_raise_data
 * 
 * Behavior:
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `raise(sig)`
 *
 * @param sig       Signal to be sent
 * @return int      error status
 */
int _eval_raise( int sig ) {
    _eval_raise_data.status ++;
    _eval_raise_data.sig = sig;

    switch( _eval_raise_data.action ) {
    case(ACTION_ERROR):    // error
        _eval_raise_data.ret = -1;
        errno = EINVAL;
        break;

    case(ACTION_LOG): // log
        datalog("raise,%d", sig );

    case(ACTION_SUCCESS):    // success
        _eval_raise_data.ret = 0;
        break;

    case(ACTION_BLOCK):
        eval_error("raise() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:    // raise signal
        _eval_raise_data.ret = raise( sig );
    }
    return _eval_raise_data.ret;
}

/**
 * @brief Global _eval_signal_data variable for the signal() function
 * 
 */
EVAL_VAR(signal);

/**
 * @brief Evaluate implementation calling of signal function
 * Requires data in global _eval_signal_data
 *
 * Note: The use of SIGPROF is reserved for the eval programs
 *
 * Behavior:
 * + `ACTION_ERROR`   - (error) Return `SIG_ERR`
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`
 * + `ACTION_SUCCESS` - (success) Return `SIG_DFL`
 * + `ACTION_DEFAULT` - Capture parameters and call `signal(signum,handler)`
 * 
 * @param signum    Signal to be handled
 * @param handler   Signal handler
 * @return          Previous value of signal handler, SIG_ERR on error
 */
sighandler_t _eval_signal(int signum, sighandler_t handler) {
    _eval_signal_data.status ++;
    _eval_signal_data.signum = signum;
    _eval_signal_data.handler = handler;

    int bad_signal = 0;

    switch( signum ) {
        case( SIGPROF ):
            eval_error("(signal) Use of SIGPROF signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGSEGV ):
            eval_error("(signal) Use of SIGSEGV signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGBUS ):
            eval_error("(signal) Use of SIGBUS signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGFPE ):
            eval_error("(signal) Use of SIGFPE signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGILL ):
            eval_error("(signal) Use of SIGILL signal is reserved for eval");
            bad_signal = 1;
            break;
    }

    if ( bad_signal ) {
        _eval_signal_data.ret = SIG_ERR;
        errno = EINVAL;
    } else {
        switch( _eval_signal_data.action ) {
        case(ACTION_ERROR):
            _eval_signal_data.ret = SIG_ERR;
            break;
        case(ACTION_LOG):
            datalog("signal,%d,%p", signum, handler );
        case(ACTION_SUCCESS):
            _eval_signal_data.ret = SIG_DFL;
            break;
        case(ACTION_BLOCK):
            eval_error("signal() called, aborting");
            siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
            break;
        default:
            _eval_signal_data.ret = signal( signum, handler );
        }
    }
    return _eval_signal_data.ret;
}

/**
 * @brief Global _eval_sigaction_data variable for the sigaction() function
 * 
 */
EVAL_VAR(sigaction);

/**
 * @brief Evaluate implementation calling of sigaction function
 * Requires data in global _eval_sigaction_data
 *
 * Note: The use of SIGPROF is reserved for the eval programs
 *
 * Behavior:
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 *                      Unless `act -> sa_flags` had the `SA_SIGINFO` bit set, the
 *                      function will be logged as if the corresponding `signal()`
 *                      call was made.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `sigaction(signum,act,oldact)`
 * 
 * @param signum    Signal to be handled
 * @param act       Signal handler
 * @param oldaction Signal handler
 * @return          0 on success, -1 on error
 */
int _eval_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {

    int err = 0;
    if ( act != NULL ) {
        if ( eval_checkconstptr( act ) != 0 ) {
            eval_error("sigaction() called with invalid pointer (act)\n");
            err++;
        }
    }

    if ( oldact != NULL ) {
        if ( eval_checkptr( oldact ) != 0 ) {
            eval_error("sigaction() called with invalid pointer (oldact)\n");
            err++;
        }
    }

    _eval_sigaction_data.status ++;
    _eval_sigaction_data.signum = signum;
    _eval_sigaction_data.act = (struct sigaction *) act;
    _eval_sigaction_data.oldact = oldact;

    int bad_signal = 0;

    switch( signum ) {
        case( SIGPROF ):
            eval_error("(sigaction) Use of SIGPROF signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGSEGV ):
            eval_error("(sigaction) Use of SIGSEGV signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGBUS ):
            eval_error("(sigaction) Use of SIGBUS signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGFPE ):
            eval_error("(sigaction) Use of SIGFPE signal is reserved for eval");
            bad_signal = 1;
            break;
        case( SIGILL ):
            eval_error("(sigaction) Use of SIGILL signal is reserved for eval");
            bad_signal = 1;
            break;
    }

    if ( bad_signal ) {
        _eval_sigaction_data.ret = -1;
        errno = EINVAL;
    } else {
        switch( _eval_sigaction_data.action ) {
        case(ACTION_ERROR):
            _eval_sigaction_data.ret = -1;
            break;
        case(ACTION_LOG):
            if ( act -> sa_flags & SA_SIGINFO ) {
                datalog("sigaction,%d,%p", signum, act->sa_sigaction );
            } else {
                datalog("signal,%d,%p", signum, act->sa_handler );
            }
        case(ACTION_SUCCESS):
            _eval_sigaction_data.ret = 0;
            break;
        case(ACTION_BLOCK):
            eval_error("sigaction() called, aborting");
            siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
            break;
        default:
            if ( ! err ) {
                _eval_sigaction_data.ret = sigaction( signum, act, oldact );
            } else {
                _eval_sigaction_data.ret = -1;
            }
        }
    }
    return _eval_sigaction_data.ret;
}

/**
 * @brief Global _eval_pause_data variable for the pause() function
 * 
 */
EVAL_VAR(pause);

/**
 * @brief Evaluate implementation calling of pause() function
 * Requires data in global _eval_pause_data
 * 
 * Behavior:
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return -1. Note: the `pause()` command always
 *                      returns -1.
 * + `ACTION_DEFAULT` - Call `pause()`
 *
 * @return          Always returns -1
 */
int _eval_pause(void) {
    _eval_pause_data.status ++;

    switch( _eval_pause_data.action ) {
    case(ACTION_LOG):
        datalog("pause");
    case(ACTION_SUCCESS):
        _eval_pause_data.ret = -1;
        break;
    case(ACTION_BLOCK):
        eval_error("pause() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;
    default:
        _eval_pause_data.ret = pause( );
    }
    return _eval_pause_data.ret;
}

/**
 * @brief Global _eval_alarm_data variable for the alarm() function
 * 
 */
EVAL_VAR(alarm);

/**
 * @brief Evaluate implementation calling of alarm() function
 * Requires data in global _eval_alarm_data
 * 
 * Behavior:
 * + `ACTION_ERROR`   - (previous alarm running) Return 1 (1s left on previous alarm)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return 0 (no previous alarm running)
 *
 * @param seconds       Number of seconds until SIGALARM
 * @return              Number of seconds remaining on previous alarm 
 */
unsigned int _eval_alarm( unsigned int seconds ) {
    _eval_alarm_data.status ++;
    _eval_alarm_data.ret = _eval_alarm_data.seconds;
    _eval_alarm_data.seconds = seconds;

    switch( _eval_alarm_data.action ) {
    case(ACTION_ERROR):
        _eval_alarm_data.ret = 1;
        break;
    case(ACTION_LOG):
        datalog("alarm,%d",seconds);
    case(ACTION_SUCCESS):
        _eval_alarm_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("alarm() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;
    default:
        _eval_alarm_data.ret = alarm( seconds );
    }
    return _eval_alarm_data.ret;
}

/**
 * @brief Global _eval_msgget_data variable for the msgget() function
 * 
 */
EVAL_VAR(msgget);

/**
 * Evaluate implementation calling of msgget function
 * 
 * Function `.action` options:
 * + `ACTION_RETRY`   - (retry) Fail on first try (ENOENT), behave as `ACTION_SUCCESS`
 *                       on 2nd try, fail on remaining attempts (EINVAL)
 * + `ACTION_CREATE`  - (must create) If `IPC_CREAT` was specified then behave as
 *                      `ACTION_SUCCESS` otherwise return -1 (ENOENT)
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return the value set in `_eval_msgget_data.msqid`
 * + `ACTION_DEFAULT` - Capture parameters and call `msgget(key,msgflg)`
 * 
 * @param key       IPC key of msg queue
 * @param msgflg    Flag controlling msg queue creation / connection
 * @return          Result or result of msgget operation
 */
int _eval_msgget(key_t key, int msgflg) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "msgget( 0x%x, 0%04o )\n", key, msgflg );
#endif

    _eval_msgget_data.status ++;

    _eval_msgget_data.key = key;
    _eval_msgget_data.msgflg = msgflg;

    switch( _eval_msgget_data.action ) {
    case(ACTION_RETRY): // error (ENOENT) on first try, inject on 2nd try
        switch( _eval_msgget_data.status ) {
        case (1):
            errno = ENOENT;
            _eval_msgget_data.ret = -1;
            break;
        case (2):
            errno = 0;
            _eval_msgget_data.ret = _eval_msgget_data.msqid;
            break;
        default:
            _eval_shmget_data.ret = -1;
            errno = EINVAL;
        }
        break;

    case(ACTION_CREATE): // inject on create only
        if ( ! (msgflg & IPC_CREAT ) ) {
            _eval_msgget_data.ret = -1;
            errno = ENOENT;
        } else {
            _eval_msgget_data.ret = _eval_msgget_data.msqid;
        }
        break;

    case(ACTION_ERROR): // error (EINVAL)
        _eval_msgget_data.ret = -1;
        errno = EINVAL;
        break;
    case(ACTION_LOG):
        datalog("msgget,%x,%d",key,msgflg);
    case(ACTION_SUCCESS): // inject
        _eval_msgget_data.ret = _eval_msgget_data.msqid;
        break;
    case(ACTION_BLOCK):
        eval_error("msgget() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;
    default:
        _eval_msgget_data.ret = msgget( key, msgflg );
    }
    
    return _eval_msgget_data.ret;
}

/**
 * @brief Global _eval_msgsnd_data variable for the msgsnd() function
 * 
 */
EVAL_VAR(msgsnd);

/**
 * Evaluate implementation calling of msgsnd function
 * Requires data in global _eval_msgsnd_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_INJECT`  - (inject) Call `msgsnd()` but using the parameters specified
 *                      in `_eval_msgsnd_data`
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return `msgsz`. If `msgp` was a valid pointer,
 *                      then the data in `*msgp` is copied to a newly allocated
 *                      buffer at `_eval_msgsnd_data.msgp`. This buffer __must__
 *                      be freed afterwards.
 * + `ACTION_DEFAULT` - Capture parameters and call `msgsnd(msqid,msgp,msgsz,msgflg)`
 * 
 * @param msqid     Queue ID to use
 * @param msgp      Pointer to message structure
 * @param msgsz     Size of message body
 * @param msgflg    
 * @return          Result of msgsnd operation
 */
int _eval_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "msgsnd( %d, %p, %zu, %d )\n", msqid, msgp, msgsz, msgflg );
#endif

    int err = 0;
    if ( eval_checkconstptr( msgp ) != 0 ) {
        eval_error("msgsnd() called with invalid pointer (msgp)\n");
        err++;
    }

    _eval_msgsnd_data.status ++;

    switch( _eval_msgsnd_data.action ) {

    case(ACTION_INJECT): // inject
        _eval_msgsnd_data.ret = msgsnd( 
            _eval_msgsnd_data.msqid, _eval_msgsnd_data.msgp,
            _eval_msgsnd_data.msgsz, _eval_msgsnd_data.msgflg );
        break;

    case(ACTION_ERROR): // error
        _eval_msgsnd_data.msqid = msqid;
        _eval_msgsnd_data.msgp = (void *) msgp;
        _eval_msgsnd_data.msgsz = msgsz;
        _eval_msgsnd_data.msgflg = msgflg;

        _eval_msgsnd_data.ret = -1;
        break;

    case(ACTION_LOG):
        datalog("msgsnd,%d,%p,%zu,%d",msqid,msgp,msgsz,msgflg);

    case(ACTION_SUCCESS): // capture
        _eval_msgsnd_data.msqid = msqid;
        _eval_msgsnd_data.msgsz = msgsz;
        _eval_msgsnd_data.msgflg = msgflg;

        if ( _eval_msgsnd_data.msgp )
            free (_eval_msgsnd_data.msgp);
        
        _eval_msgsnd_data.msgp = NULL;

        if ( ! err ) {
            size_t bytes = sizeof(long);
            if ( msgsz >= 0 ) bytes += msgsz;
            _eval_msgsnd_data.msgp = malloc( bytes );
            memcpy( _eval_msgsnd_data.msgp, msgp, bytes );
        }

        _eval_msgsnd_data.ret = 0;
        break;

    case(ACTION_BLOCK):
        eval_error("msgsnd() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        _eval_msgsnd_data.msqid = msqid;
        _eval_msgsnd_data.msgp = (void *) msgp;
        _eval_msgsnd_data.msgsz = msgsz;
        _eval_msgsnd_data.msgflg = msgflg;

        if ( ! err ) {
            _eval_msgsnd_data.ret = msgsnd( msqid, msgp, msgsz, msgflg );
        } else {
            _eval_msgsnd_data.ret = -1;
        }
        break;
    }

    return _eval_msgsnd_data.ret;
}

/**
 * @brief Global _eval_msgrcv_data variable for the msgrcv() function
 * 
 */
EVAL_VAR(msgrcv);

/**
 * Evaluate implementation calling of msgsnd function
 * Requires data in global _eval_msgrcv_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_INJECT`  - (inject) Copy data in `*_eval_msgsnd_data.msgp` to `*msgp`.
 *                      The pointer `msgp` is checked before copying.
 * + `ACTION_ERROR`   - (error) Return -1
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return `msgsz`.
 * + `ACTION_DEFAULT` - Capture parameters and call `msgrcv( msqid, msgp, msgsz, msgtyp, msgflg )`
 * 
 * @param msqid     Queue ID to use
 * @param msgp      Pointer to message structure
 * @param msgsz     Size of message body
 * @param msgflg    
 * @return          Evalation result or result of msgsnd operation
 */
ssize_t _eval_msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "msgrcv( %d, %p, %zu, %ld, %d )\n", msqid, msgp, msgsz, msgtyp, msgflg );
#endif

    int err = 0;
    if ( eval_checkptr( msgp ) != 0 ) {
        eval_error("msgrcv() called with invalid pointer (msgp)\n");
        err++;
    }

    _eval_msgrcv_data.status ++;

    switch( _eval_msgrcv_data.action ) {

    case(ACTION_INJECT): // inject
        _eval_msgrcv_data.msqid = msqid;
        _eval_msgrcv_data.msgtyp = msgtyp;
        _eval_msgrcv_data.msgflg = msgflg;

        if ( !err ) {
            size_t bytes = sizeof(long);
            if ( msgsz >= _eval_msgrcv_data.msgsz ) {
                bytes += _eval_msgrcv_data.msgsz;
            } else if ( msgsz >= 0 ) {
                bytes += msgsz;
            }
            memcpy( msgp, _eval_msgrcv_data.msgp, bytes );
        }

        _eval_msgrcv_data.msgp = msgp;
        _eval_msgrcv_data.msgsz = msgsz;
        _eval_msgrcv_data.ret = msgsz;

        break;

    case(ACTION_ERROR): // error
        _eval_msgrcv_data.msqid = msqid;
        _eval_msgrcv_data.msgp = msgp;
        _eval_msgrcv_data.msgsz = msgsz;
        _eval_msgrcv_data.msgtyp = msgtyp;
        _eval_msgrcv_data.msgflg = msgflg;

        _eval_msgrcv_data.ret = -1;
        break;

    case(ACTION_LOG):
        datalog("msgrcv,%d,%p,%zu,%ld,%d",msqid,msgp,msgsz,msgtyp,msgflg);

    case(ACTION_SUCCESS): // capture
        _eval_msgrcv_data.msqid = msqid;
        _eval_msgrcv_data.msgp = msgp;
        _eval_msgrcv_data.msgsz = msgsz;
        _eval_msgrcv_data.msgtyp = msgtyp;
        _eval_msgrcv_data.msgflg = msgflg;
        
        _eval_msgrcv_data.ret = msgsz;

        break;

    case(ACTION_BLOCK):
        eval_error("msgrcv() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        _eval_msgrcv_data.msqid = msqid;
        _eval_msgrcv_data.msgp = msgp;
        _eval_msgrcv_data.msgsz = msgsz;
        _eval_msgrcv_data.msgtyp = msgtyp;
        _eval_msgrcv_data.msgflg = msgflg;
        
        if ( ! err ) {
            _eval_msgrcv_data.ret =  msgrcv( msqid, msgp, msgsz, msgtyp, msgflg );
        } else {
            _eval_msgrcv_data.ret = -1;
        }
    }
    return _eval_msgrcv_data.ret;

}

/**
 * @brief Global _eval_msgctl_data variable for the msgctl() function
 * 
 */
EVAL_VAR(msgctl);

/**
 * Evaluate implementation calling of msgctl function
 * Requires data in global _eval_msgctl_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `msgctl( msqid, cmd, buf )`
 * 
 * @param msqid     Message queue id
 * @param cmd       Command to perform
 * @param buf       struct msqid_ds for input / output of command
 * @return int      success
 */
int _eval_msgctl(int msqid, int cmd, struct msqid_ds *buf) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "msgctl( %d, %d, %p )\n", msqid, cmd, (void *) buf );
#endif

    _eval_msgctl_data.status ++;
    _eval_msgctl_data.msqid = msqid;
    _eval_msgctl_data.cmd = cmd;
    _eval_msgctl_data.buf = buf;
    
    switch( _eval_msgctl_data.action ) {

    case(ACTION_ERROR):
        _eval_msgctl_data.ret = -1;
        errno = EINVAL;
        break;
    case(ACTION_LOG):
        datalog("msgctl,%d,%d,%p",msqid,cmd,(void*)buf);
    case(ACTION_SUCCESS):
        _eval_msgctl_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("msgctl() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        _eval_msgctl_data.ret = msgctl( msqid, cmd, buf );
    }
    

    return _eval_msgctl_data.ret;
}

/**
 * @brief Global _eval_semget_data variable for the semget() function
 * 
 */
EVAL_VAR(semget);

/**
 * Evaluate implementation calling of semget function
 * Requires data in global _eval_semget_data
 * 
 * @param key       IPC key
 * @param nsems     Number of semaphores in the array
 * @param semflg    Flags for semaphore creation / connection
 * @return          semaphore id
 */
int _eval_semget(key_t key, int nsems, int semflg) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "semget( 0x%x, %d, 0%o )\n", key, nsems, semflg );
#endif

    _eval_semget_data.status ++;

    _eval_semget_data.key = key;
    _eval_semget_data.nsems = nsems;
    _eval_semget_data.semflg = semflg;

    switch( _eval_semget_data.action ) {
        case(ACTION_RETRY): // error (ENOENT) on first try, inject on 2nd try
            switch( _eval_semget_data.status ) {
            case (1):
                errno = ENOENT;
                _eval_semget_data.ret = -1;
                break;
            case (2):
                errno = 0;
                _eval_semget_data.ret = _eval_semget_data.semid;
                break;
            default:
                _eval_semget_data.ret = -1;
                errno = EINVAL;
            }
            break;
        
        case(ACTION_CREATE): // inject on create only
            if ( nsems == 0  && ( semflg & IPC_CREAT ) ) {
                _eval_semget_data.ret = -1;
                errno = EINVAL;
            } else {
                _eval_semget_data.ret = _eval_semget_data.semid;
            }
            break;

        case(ACTION_ERROR): // error (EINVAL)
            _eval_semget_data.ret = -1;
            errno = EINVAL;
            break;

        case(ACTION_LOG):
            datalog("semget,%x,%d,%o",key,nsems,semflg);

        case(ACTION_SUCCESS): // inject
            _eval_semget_data.ret = _eval_semget_data.semid;
            break;

        case(ACTION_BLOCK):
            eval_error("semget() called, aborting");
            siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
            break;

        default:
            _eval_semget_data.ret = semget( key, nsems, semflg );
    }
    
    return _eval_semget_data.ret;
}

/**
 * @brief Global _eval_semctl_data variable for the semctl() function
 * 
 */
EVAL_VAR(semctl);

/**
 * @brief Evaluate implementation calling of semctl function
 * Requires data in global _eval_semctl_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return the 0
 * + `ACTION_DEFAULT` - Capture parameters and call `semctl(semid,semnum,cmd,...)`
 * 
 * @param semid     semaphore id
 * @param sops      Semaphore operations
 * @param nsops     nsops
 * @return          semop result
 */
int _eval_semctl(int semid, int semnum, int cmd, ... ) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "semctl( %d, %d, %d, ... )\n", semid, semnum, cmd );
#endif

    _eval_semctl_data.status ++;

    _eval_semctl_data.semid = semid;
    _eval_semctl_data.semnum = semnum;
    _eval_semctl_data.cmd = cmd;

    switch (cmd) {
    case(SETVAL):
    case(IPC_STAT):
    case(IPC_SET):
    case(GETALL):
    case(SETALL): 
        {
            va_list valist;
            va_start(valist, cmd );
            _eval_semctl_data.arg = va_arg(valist, union semun );
            va_end(valist);
        }
    }

    switch (_eval_semctl_data.action) {

    case(ACTION_ERROR): // error
        _eval_semctl_data.ret = -1;
        errno = EINVAL;
        break;
    case(ACTION_LOG): // log
        datalog("semctl,%d,%d,%d", semid, semnum, cmd );
    case(ACTION_SUCCESS): // success
        _eval_semctl_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("semctl() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        switch (cmd) {
        case(SETVAL):
        case(IPC_STAT):
        case(IPC_SET):
        case(GETALL):
        case(SETALL):
            _eval_semctl_data.ret = semctl( semid, semnum, cmd, _eval_semctl_data.arg );
            break;
        default:
            _eval_semctl_data.ret = semctl( semid, semnum, cmd );
        }
    }

    return _eval_semctl_data.ret;
}

/**
 * @brief Global _eval_semop_data variable for the semop() function
 * 
 */
EVAL_VAR(semop);

/**
 * @brief Evaluate implementation calling of semop function
 * Requires data in global _eval_semop_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return the 0
 * + `ACTION_DEFAULT` - Capture parameters and call `semop(semid,sops,nsops)`
 * 
 * @param semid     semaphore id
 * @param sops      Semaphore operations
 * @param nsops     nsops
 * @return          semop result
 */
int _eval_semop(int semid, struct sembuf *sops, size_t nsops) {

    int err = 0;
    if ( eval_checkptr( sops ) != 0 ) {
        eval_error("semop() called with invalid pointer (sops)\n");
        err++;
    }

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "semop( %d, %p, %zu )\n", semid, (void *) sops ,nsops );
    if ( nsops > 0 && !err ) {
        for( int i = 0; i < nsops; i++ ) {
            printf( "    ↳ op(%d) = {num:%d, op:%+d, flag:%d}\n", i, sops[i].sem_num, sops[i].sem_op, sops[i].sem_flg );
        }
    }
#endif

    _eval_semop_data.status ++;

    _eval_semop_data.sops = sops;
    _eval_semop_data.nsops = nsops;

    switch( _eval_semop_data.action ) {
    case(ACTION_ERROR): // error
        _eval_semop_data.ret = -1;
        errno = EINVAL;
        break;
    case(ACTION_LOG):
        if ( !err ) {
            for( int i = 0; i < nsops; i++ ) {
                datalog("semop,%d,%d,%d,%d", semid, sops[i].sem_num, sops[i].sem_op, sops[i].sem_flg );
            }
        }
    case(ACTION_SUCCESS): // success
        _eval_semop_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("semop() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_semop_data.ret = semop( semid, sops, nsops );
        } else {
            _eval_semop_data.ret = -1;
            errno = EINVAL;
        }
    }

    

    return _eval_semop_data.ret;
}

/**
 * @brief Global _eval_shmget_data variable for the shmget() function
 * 
 */
EVAL_VAR(shmget);

/**
 * @brief Evaluate implementation calling of shmget function
 * Requires data in global _eval_semget_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_RETRY`   - (retry) Fail on first try (ENOENT), behave as `ACTION_SUCCESS`
 *                      on 2nd try, fail on remaining attempts (EINVAL)
 * + `ACTION_CREATE`  - (must create) If `IPC_CREAT` was specified then behave as
 *                      `ACTION_SUCCESS` otherwise return -1 (ENOENT)
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return the value set in `_eval_shmget_data.shmid`
 * + `ACTION_DEFAULT` - Capture parameters and call `shmget( key, size, shmflg )`
 * 
 * @param key       IPC key
 * @param size      Size of shared memory region
 * @param semflg    Flags for shmem creation / connection
 * @return          shm id
 */
int _eval_shmget(key_t key, size_t size, int shmflg) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "shmget( 0x%x, %zu, 0%04o )\n", key, size, shmflg );
#endif

    _eval_shmget_data.status ++;

    _eval_shmget_data.key = key;
    _eval_shmget_data.size = size;
    _eval_shmget_data.shmflg = shmflg;

    switch( _eval_shmget_data.action ) {
    case(ACTION_RETRY): // error (ENOENT) on first try, inject on 2nd try
        switch( _eval_shmget_data.status ) {
        case (1):
            errno = ENOENT;
            _eval_shmget_data.ret = -1;
            break;
        case (2):
            if ( size == 0  && ( shmflg & IPC_CREAT ) ) {
                    _eval_shmget_data.ret = -1;
            } else {
                _eval_shmget_data.ret = _eval_shmget_data.shmid;
            }
            break;
        default:
            _eval_shmget_data.ret = -1;
        }
        break;
    case(ACTION_CREATE): // inject on create only
        if ( size == 0  && ( shmflg & IPC_CREAT ) ) {
                _eval_shmget_data.ret = -1;
        } else {
            _eval_shmget_data.ret = _eval_shmget_data.shmid;
        }
        break;
    case(ACTION_ERROR): // error
        _eval_shmget_data.ret = -1;
        errno = EINVAL;
        break;
    case(ACTION_LOG): // log
        datalog("shmget,%x,%ld,%d", key, size, shmflg );
    case(ACTION_SUCCESS): // success
        _eval_shmget_data.ret = _eval_shmget_data.shmid;
        break;
    case(ACTION_BLOCK):
        eval_error("shmget() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;
    default:
        _eval_shmget_data.shmid = shmget( key, size, shmflg );
        _eval_shmget_data.ret = _eval_shmget_data.shmid;
    }

    return _eval_shmget_data.ret;
}

/**
 * @brief Global _eval_shmat_data variable for the shmat() function
 * 
 */
EVAL_VAR(shmat);

/**
 * @brief Evaluate implementation calling of shmat function
 * Requires data in global _eval_shmat_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return `(void *) -1` (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return the value set in `_eval_shmat_data.shmaddr`
 * + `ACTION_DEFAULT` - Capture parameters and call `shmat( shmid, shmaddr, shmflg )`
 * 
 * @param shmid     Shared memory ID
 * @param shmaddr   Choice for logical address
 * @param shmflg    Flags for address choice
 * @return          Logical address of shared memory region
 */
void *_eval_shmat( int shmid, const void *shmaddr, int shmflg) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "shmat( %d, %p, %d )\n", shmid, shmaddr, shmflg );
#endif

    _eval_shmat_data.status ++;
    _eval_shmat_data.shmid = shmid;
    _eval_shmat_data.shmaddr = (void *) shmaddr;
    _eval_shmat_data.shmflg = shmflg;

    switch( _eval_shmat_data.action ) {
    case(ACTION_ERROR):
        _eval_shmat_data.ret = (void *) -1;
        errno=EINVAL;
        break;
    case(ACTION_LOG): // log
        datalog("shmat,%d,%p,%d", shmid, shmaddr, shmflg );
    case(ACTION_SUCCESS): // inject
        _eval_shmat_data.ret = _eval_shmat_data.shmaddr;
        break;

    case(ACTION_BLOCK):
        eval_error("shmat() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        _eval_shmat_data.shmid = shmid;
        _eval_shmat_data.shmaddr = (void *) shmaddr;
        _eval_shmat_data.shmflg = shmflg;

        _eval_shmat_data.ret = shmat( shmid, shmaddr, shmflg );
    }

    return _eval_shmat_data.ret;
}

/**
 * @brief Global _eval_shmdt_data variable for the shmdt() function
 * 
 */
EVAL_VAR(shmdt);

/**
 * Evaluate implementation calling of shmdt function
 * Requires data in global _eval_shmdt_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `shmdt( shmaddr )`
 * 
 * @param shmaddr   Logical address of shared memory region to detach
 * @return          success
 */
int _eval_shmdt(const void *shmaddr) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "shmdt( %p )\n", shmaddr );
#endif

    _eval_shmdt_data.status ++;
    _eval_shmdt_data.shmaddr = (void *) shmaddr;
    
    switch( _eval_shmdt_data.action ) {

    case( ACTION_ERROR ): // error
        _eval_shmdt_data.ret = -1;
        errno = EINVAL;
        break;

    case( ACTION_LOG ): // log
        datalog("shmdt,%p", shmaddr );

    case( ACTION_SUCCESS ): // success
        _eval_shmdt_data.ret = 0;
        break;

    case(ACTION_BLOCK):
        eval_error("shmdt() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        _eval_shmdt_data.ret = shmdt( shmaddr );
    }

    return _eval_shmdt_data.ret;
}

/**
 * @brief Global _eval_shmctl_data variable for the shmctl() function
 * 
 */
EVAL_VAR(shmctl);

/**
 * Evaluate implementation calling of shmctl function
 * Requires data in global _eval_shmctl_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `shmdt( shmaddr )`
 * 
 * @param shmid     Shared memory ID
 * @param cmd       Command to perform
 * @param buf       struct shmid_ds for input / output of command
 * @return          success
 */
int _eval_shmctl(int shmid, int cmd, struct shmid_ds *buf) {

#ifdef _EVAL_DEBUG
    printf( _EVAL_DEBUG_PREFIX "shmctl( %d, %d, %p )\n", shmid, cmd, (void *) buf );
#endif

    _eval_shmctl_data.status ++;
    _eval_shmctl_data.shmid = shmid;
    _eval_shmctl_data.cmd = cmd;
    _eval_shmctl_data.buf = buf;

    switch( _eval_shmctl_data.action ) {
    case( ACTION_ERROR ):
        _eval_shmctl_data.ret = -1;
        errno = EINVAL;
        break;
    case( ACTION_LOG ): //log
        datalog("shmctl,%d,%d,%p", shmid, cmd, buf );
    case( ACTION_SUCCESS ): // success
        _eval_shmctl_data.ret = 0;
        break;
    case( ACTION_BLOCK ):
        eval_error("shmctl() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
    _eval_shmctl_data.ret = shmctl( shmid, cmd, buf );
    }

    return _eval_shmctl_data.ret;
}

/**
 * @brief Global _eval_mkfifo_data variable for the mkfifo() function
 * 
 */
EVAL_VAR(mkfifo);

/**
 * @brief Evaluate implementation calling of mkfifo function
 * Requires data in global _eval_mkfifo_data
 *
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `mkfifo( path, mode )`
 *
 * @param path      Path to FIFO
 * @param mode      Creation mode
 * @return int      Success
 */
int _eval_mkfifo(const char *path, mode_t mode) {
    
    _eval_mkfifo_data.status ++;
    _eval_mkfifo_data.mode = mode;

    int err = 0;
    if ( eval_checkconstptr(path) ) {
        eval_error("mkfifo(path,mode) invalid path");
        _eval_mkfifo_data.path[0] = 0;
        err++;
    } else {
        strncpy( _eval_mkfifo_data.path, path, PATH_MAX );
    }

    switch( _eval_mkfifo_data.action ) {
        case( ACTION_ERROR ):
            _eval_mkfifo_data.ret = -1;
            errno = EINVAL;
            break;
        case( ACTION_LOG ): //log
            datalog("mkfifo,%s,%o", _eval_mkfifo_data.path, mode );
        case( ACTION_SUCCESS ):
            _eval_mkfifo_data.ret = 0;
            break;
        case(ACTION_BLOCK):
            eval_error("mkfifo() called, aborting");
            siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
            break;
        default:
            if ( !err ) {
                _eval_mkfifo_data.ret = mkfifo( path, mode );
            } else {
                _eval_mkfifo_data.ret = -1;
                errno = EINVAL;
            }
    }
    return _eval_mkfifo_data.ret;
}

/**
 * @brief Global _eval_mkfifo_data variable for the S_ISFIFO() macro
 * 
 */
EVAL_VAR(isfifo);

/**
 * @brief Evaluate implementation calling of S_ISFIFO macro
 *
 * Requires data in global _eval_isfifo_data
 *
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (not a FIFO) Return 0
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (is a FIFO) Return 1
 * + `ACTION_DEFAULT` - Capture parameters and call `S_ISFIFO( mode )`
 *
 * @param mode  File mode from stat or fstat
 * @return      For action = 1 always returns true, for action = 0
 *              has the same behavior as the S_ISFIFO macro
 */
int _eval_isfifo(mode_t mode) {
    
    _eval_isfifo_data.status ++;
    _eval_isfifo_data.mode = mode;

    switch( _eval_isfifo_data.action ) {
        case(ACTION_ERROR):
            _eval_isfifo_data.ret = 0;
            break;
        case( ACTION_LOG ): //log
            datalog("S_ISFIFO,%o", mode );
        case(ACTION_SUCCESS):
            _eval_isfifo_data.ret = 1;
            break;
        case(ACTION_BLOCK):
            eval_error("S_ISFIFO() called, aborting");
            siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
            break;
        default:
            _eval_isfifo_data.ret = S_ISFIFO(mode);
    }
    return _eval_isfifo_data.ret;
}

/**
 * @brief Global _eval_remove_data variable for the remove() function
 * 
 */
EVAL_VAR(remove);

/**
 * @brief Evaluate implementation calling of remove() function
 *
 * Requires data in global _eval_remove_data
 *
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 *                      Note that the function call is logged as if the
 *                      corresponding `unlink()` function call was made.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `remove( path )`
 *
 * @param path      File name
 * @return int      0 on success, -1 on error
 */
int _eval_remove(const char * path) {
    _eval_remove_data.status ++;

    int err = 0;
    if ( eval_checkconstptr(path) ) {
        eval_error("remove(path) invalid path");
        _eval_remove_data.path[0] = 0;
        err++;
    } else {
        strncpy( _eval_remove_data.path, path, PATH_MAX );
    }

    _eval_remove_data.ret = 0;

    switch( _eval_remove_data.action ) {
    case(ACTION_ERROR): // error
        _eval_remove_data.ret = -1;
        errno = EINVAL;
        break;
    case( ACTION_LOG ): //log
        // We use "unlink" deliberately
        datalog("unlink,%s", _eval_remove_data.path );
    case(ACTION_SUCCESS): // success
        _eval_remove_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("remove() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( !err ) {
            _eval_remove_data.ret = remove( path );
        } else {
            _eval_remove_data.ret = -1;
            errno = EINVAL;
        }
    }
    return _eval_remove_data.ret;
}

/**
 * @brief Global _eval_unlink_data variable for the unlink() function
 * 
 */
EVAL_VAR(unlink);

/**
 * @brief Evaluate implementation calling of unlink() function
 *
 * Requires data in global _eval_unlink_data
 *
 *  Behavior:
 *    .action = 3 - Log call parameters and return 0 without calling unlink().
 *    .action = 2 - Return -1 without calling unlink()
 *    .action = 1 - Return 0 without calling unlink()
 *    default     - Call unlink()
 * 
 * @param path      File name
 * @return int      0 on success, -1 on error
 */
int _eval_unlink(const char * path) {
    _eval_unlink_data.status ++;
    _eval_unlink_data.ret = 0;

    int err = 0;
    if ( eval_checkconstptr(path) ) {
        eval_error("unlink(path) invalid path");
        _eval_unlink_data.path[0] = 0;
        err++;
    } else {
        strncpy( _eval_unlink_data.path, path, PATH_MAX );
    }

    switch( _eval_unlink_data.action ) {
    case(ACTION_ERROR):
        _eval_unlink_data.ret = -1;
        errno = EINVAL;
        break;
    case(ACTION_LOG):
        datalog("unlink,%s", _eval_unlink_data.path );
    case(ACTION_SUCCESS):
        _eval_unlink_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("unlink() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( !err ) {
            _eval_unlink_data.ret = unlink( path );
        } else {
            _eval_unlink_data.ret = -1;
            errno = EINVAL;
        }
    }
    return _eval_unlink_data.ret;
}

/**
 * @brief Global _eval_atoi_data variable for the atoi() function
 * 
 */
EVAL_VAR(atoi);

/**
 * @brief Evaluate implementation calling of atoi() function
 *
 * Requires data in global _eval_atoi_data
 *
 * The nptr pointer is checked before calling atoi(). On failure the value -1
 * is returned.
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return INT32_MIN
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return previous `_eval_atoi_data.ret` value
 * + `ACTION_DEFAULT` - Capture parameters and call `atoi( nptr )`
 * 
 * @param nptr      String to be converted to integer
 * @return int      Converted value
 */
int _eval_atoi( const char *nptr ) {
    _eval_atoi_data.status++;

    _eval_atoi_data.ret = -1;

    int err = 0;
    if ( ! eval_checkconstptr( nptr ) ) {
        eval_error("atoi(nptr) called with invalid nptr");
        _eval_atoi_data.nptr[0] = 0;
        err++;
    } else {
        strncpy( _eval_atoi_data.nptr, nptr, 32 );
        // atoi() is unsafe, we first check if the supplied string
        // has a length of up to 32 characters
        eval_error("atoi(nptr) called with invalid string");
        err++;
    }

    switch( _eval_atoi_data.action ) {
    case( ACTION_ERROR ):
        _eval_atoi_data.ret = INT32_MIN;
        break;
    case( ACTION_LOG ):
        datalog("atoi,%s", _eval_atoi_data.nptr );
    case( ACTION_SUCCESS ):
        // Keep _eval_atoi_data.ret value
        // _eval_atoi_data.ret;
        break;
    case(ACTION_BLOCK):
        eval_error("atoi() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_atoi_data.ret = atoi( nptr );
        }  else {
            _eval_atoi_data.ret = -1;
        }
    }

    return _eval_atoi_data.ret;
}

/**
 * @brief Global _eval_fclose_data variable for the fclose() function
 * 
 */
EVAL_VAR(fclose);

/**
 * @brief Evaluate implementation calling of fclose() function
 *
 * Requires data in global _eval_fclose_data
 *
 * @param stream 
 * @return int      0 on success, < 0 on error
 */
int _eval_fclose( FILE* stream ) {
    _eval_fclose_data.status++;
    _eval_fclose_data.ret = -1;
    _eval_fclose_data.stream = stream;

    int err = 0;
    if ( ! eval_checkptr( stream ) ) {
        eval_error("fclose(stream) called with invalid stream");
        err++;
    }

    switch( _eval_fclose_data.action ) {

    case( ACTION_ERROR ): // error
        _eval_fclose_data.ret = EOF;
        errno = EINVAL;
        break;
    case(ACTION_LOG):
        datalog("fclose,%p", stream );
    case( ACTION_SUCCESS ): // success
        _eval_fclose_data.ret = 0;
        break;
    case(ACTION_BLOCK):
        eval_error("fclose() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_fclose_data.ret = fclose(stream);
        } else {
            _eval_fclose_data.ret = EOF;
            errno = EINVAL;
        }
    }

    return _eval_fclose_data.ret;
}

/**
 * @brief Global _eval_fread_data variable for the fread() function
 * 
 */
EVAL_VAR(fread);

/**
 * @brief  Evaluate implementation calling of fread() function
 *
 * Requires data in global _eval_fread_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return 0 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return `nmemb`
 * + `ACTION_DEFAULT` - Capture parameters and call `fread( ptr, size, nmemb, stream )`
 * 
 * @param ptr       Target memory location
 * @param size      Item size in bytes
 * @param nmemb     Number of items
 * @param stream    Stream to read data from
 * @return size_t   Number of items read
 */
size_t _eval_fread(void *restrict ptr, size_t size, size_t nmemb,
                    FILE *restrict stream) {

    _eval_fread_data.status++;

    _eval_fread_data.ptr = ptr;
    _eval_fread_data.size = size;
    _eval_fread_data.nmemb = nmemb;
    _eval_fread_data.stream = stream;


    // catch common errors
    int err = 0;
    if ( (void *) ptr == (void *) stream ) {
        eval_error("fread(ptr,size,nmemb,stream) ptr must not have the same value as stream");
        err++;
    } else {
        if ( eval_checkptr(ptr) ) {
            eval_error("fread(ptr,size,nmemb,stream) invalid ptr (%p)", ptr);
            err++;
        }

        if ( eval_checkptr(stream) ) {
            eval_error("fread(ptr,size,nmemb,stream) invalid stream (%p)", stream);
            err++;
        }
    }

    if ( size * nmemb <= 0  ) {
        eval_error("fread(ptr,size,nmemb,stream) invalid size or nmemb");
        err++;
    }

    _eval_fread_data.ret = 0;
        
    switch( _eval_fread_data.action ) {

    case( ACTION_ERROR ):
        _eval_fread_data.ret = 0;
        errno = EINVAL;
        break;
    case( ACTION_LOG ):
        datalog("fread,%p,%ld,%ld,%p", ptr, size, nmemb, stream );
    case( ACTION_SUCCESS ):
        _eval_fread_data.ret = nmemb;
        break;

    case(ACTION_BLOCK):
        eval_error("fread() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_fread_data.ret = fread( ptr, size, nmemb, stream );
        } else {
            _eval_fread_data.ret = 0;
            errno = EINVAL;
        }
    }

    return _eval_fread_data.ret;

}

/**
 * @brief Global _eval_fwrite_data variable for the fwrite() function
 * 
 */
EVAL_VAR(fwrite);

/**
 * @brief  Evaluate implementation calling of fwrite() function
 *
 * Requires data in global _eval_fwrite_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return 0 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return `nmemb`
 * + `ACTION_DEFAULT` - Capture parameters and call `fwrite( ptr, size, nmemb, stream )`
 * 
 * @param ptr       Source memory location
 * @param size      Item size in bytes
 * @param nmemb     Number of items
 * @param stream    Stream to write data to
 * @return size_t   Number of items written
 */
size_t _eval_fwrite(const void *ptr, size_t size, size_t nmemb,
                     FILE *stream) {

    _eval_fwrite_data.status++;
    _eval_fwrite_data.ptr = (void *) ptr;
    _eval_fwrite_data.size = size;
    _eval_fwrite_data.nmemb = nmemb;
    _eval_fwrite_data.stream = stream;


    // catch common errors
    int err = 0;
    if ( (void *) ptr == (void *) stream ) {
        eval_error("fwrite(ptr,size,nmemb,stream) ptr must not have the same value as stream");
        err++;
    } else {
        if ( eval_checkptr(_eval_fwrite_data.ptr) ) {
            eval_error("fwrite(ptr,size,nmemb,stream) invalid ptr (%p)", ptr);
            err++;
        }

        if ( eval_checkptr(stream) ) {
            eval_error("fwrite(ptr,size,nmemb,stream) invalid stream (%p)", stream);
            err++;
        }
    }

    if ( size * nmemb <= 0  ) {
        eval_error("fwrite(ptr,size,nmemb,stream) invalid size or nmemb");
        err++;
    }

    _eval_fwrite_data.ret = 0;
        
    switch( _eval_fwrite_data.action ) {

    case( ACTION_ERROR ):
        _eval_fwrite_data.ret = 0;
        errno = EINVAL;
        break;
    case( ACTION_LOG ):
        datalog("fwrite,%p,%ld,%ld,%p", ptr, size, nmemb, stream );
    case( ACTION_SUCCESS ):
        _eval_fwrite_data.ret = nmemb;
        break;

    case(ACTION_BLOCK):
        eval_error("fwrite() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            _eval_fwrite_data.ret = fwrite( ptr, size, nmemb, stream );
        } else {
            _eval_fwrite_data.ret = 0;
            errno = EINVAL;
        }
    }

    return _eval_fwrite_data.ret;
}

/**
 * @brief Global _eval_fseek_data variable for the fseek() function
 * 
 */
EVAL_VAR(fseek);

/**
 * @brief Evaluate implementation calling of fseek() function
 *
 * Requires data in global _eval_fseek_data
 * 
 * Function `.action` options:
 * 
 * + `ACTION_ERROR`   - (error) Return -1 (EINVAL)
 * + `ACTION_LOG`     - (log) Log function call and proceed with `ACTION_SUCCESS`.
 * + `ACTION_SUCCESS` - (success) Return 0
 * + `ACTION_DEFAULT` - Capture parameters and call `fseek( stream, offset, whence )`
 * 
 * @param stream 
 * @param offset 
 * @param whence 
 * @return int 
 */
int _eval_fseek(FILE *stream, long offset, int whence) {

    _eval_fseek_data.status++;
    _eval_fseek_data.stream = stream;
    _eval_fseek_data.offset = offset;
    _eval_fseek_data.whence = whence;

    // catch common errors
    int err = 0;
    if ( eval_checkptr( stream ) ) {
        eval_error("fseek(stream, offset, whence) invalid stream");
        err ++;
    }

    switch( whence ) {
    case(SEEK_SET):
    case(SEEK_CUR):
    case(SEEK_END):
        break;
    default: 
        eval_error("fseek(stream, offset, whence) invalid value for whence");
        err ++;
    }

    _eval_fseek_data.ret = -1;
    switch( _eval_fseek_data.action ) {

    case( ACTION_ERROR ): // error
        _eval_fseek_data.ret = -1;
        errno = EINVAL;
        break;
    case( ACTION_LOG ):
        datalog("fseek,%p,%ld,%p", stream, offset, whence );
    case( ACTION_SUCCESS ): // success
        _eval_fseek_data.ret = 0;
        break;

    case(ACTION_BLOCK):
        eval_error("fseek() called, aborting");
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( !err ) {
            _eval_fseek_data.ret = fseek( stream, offset, whence );
        } else {
            _eval_fseek_data.ret = -1;
            errno = EINVAL;
        }
    }
    return _eval_fseek_data.ret;
}

/**
 * @brief Global _eval_execl_data variable for the execl() function
 * 
 */
EVAL_VAR(execl);

/**
 * @brief Evaluate implementation calling of execl() function
 *
 * Requires data in global _eval_execl_data
 *
 * The default settings implemented by the eval_reset() function will prevent
 * the execl() function from being called, instead issuing an error and
 * terminating the code.
 *
 * The function can be made to call `execl()` by setting `.action = 0` but
 * unless the call fails this will replace the current process with a new one.
 * Unless a new process was forked before this happens, this will of course
 * stop the execution of the current process.
 * 
 * @param path      Path to the binary being called
 * @param ...       Additional parameter to the binary being calledf
 * @return int      -1 on error. The function does not return on success.
 */
int _eval_execl(const char *path, ... ) {

    _eval_execl_data.status ++;

    int err = 0;
    if ( eval_checkconstptr( path ) ) {
        eval_error("execl(path, ...) invalid path");
        _eval_execl_data.path[0] = 0;
        err++ ;
    } else {
        strncpy( _eval_execl_data.path, path, PATH_MAX );
    }

    switch( _eval_execl_data.action ) {

    
    case( ACTION_LOG ):
        datalog("execl,%s", _eval_execl_data.path );
    case(ACTION_ERROR):
        _eval_execl_data.ret = -1;
        break;

    case(ACTION_BLOCK):
        eval_error("execl() called, aborting");
        _eval_execl_data.ret = -1;
        siglongjmp(_eval_env.jmp, EVAL_CATCH_BLOCKED );
        break;

    default:
        if ( ! err ) {
            va_list args;
            va_start( args, path );
            _eval_execl_data.ret = execv( path, (char **)args );
            va_end( args );
        } else {
            _eval_execl_data.ret = -1;
        }
    }
    return _eval_execl_data.ret;
}

/**
 * @brief Sets all _eval_*_data variables to 0
 *
 * This resets the status counter and sets the default action for all functions.
 *
 * It also sets the default timeout behavior.
 * 
 */
void eval_reset_vars( void ) {

    // To disable timeout by default compile with -DEVAL_TIMEOUT=0
    _eval_env.timeout = EVAL_TIMEOUT;

    RESET_VAR(exit);
    RESET_VAR(abort);

    RESET_VAR(sleep);

    RESET_VAR(fork);
    RESET_VAR(wait);
    RESET_VAR(waitpid);

    RESET_VAR(kill);
    RESET_VAR(raise);
    RESET_VAR(signal);
    RESET_VAR(sigaction);
    RESET_VAR(pause);
    RESET_VAR(alarm);

    RESET_VAR(msgget);
    RESET_VAR(msgsnd);
    RESET_VAR(msgrcv);
    RESET_VAR(msgctl);

    RESET_VAR(semget);
    RESET_VAR(semctl);
    RESET_VAR(semop);

    RESET_VAR(shmget);
    RESET_VAR(shmat);
    RESET_VAR(shmdt);
    RESET_VAR(shmctl);

    RESET_VAR(mkfifo);
    RESET_VAR(isfifo);

    RESET_VAR(remove);
    RESET_VAR(unlink);

    RESET_VAR(atoi);
    RESET_VAR(fclose);
    RESET_VAR(execl);
    RESET_VAR(fread);
    RESET_VAR(fwrite);
    RESET_VAR(fseek);
}

/**
 * @brief Sets default values for evaluation variables
 * 
 * This sets the default behavior for all eval.c functions and clears all counters.
 * 
 * Specifically:
 *  1 - Resets all _eval_*_data variables to 0, including the status and action fields
 *  2 - Resets all stats counters
 *  3 - Sets the timeout time to EVAL_TIMEOUT
 *  4 - Block execution of pause(), execl(), wait() and waitpid()
 *  5 - Prevent signals to self
 */
void eval_reset( void ) {
    // Reset all values
    // Except for the exit() function the default behavior is to
    // capture function parameters and then call function
    eval_reset_vars();

    // Abort test whenever pause() or execl() are called
    _eval_pause_data.action = ACTION_BLOCK; 
    _eval_execl_data.action = ACTION_BLOCK; 

    // Abort test whenever wait() or waitpid() are called
    _eval_wait_data.action = ACTION_BLOCK; 
    _eval_waitpid_data.action = ACTION_BLOCK; 

    // Prevent signals to self
    _eval_raise_data.action = ACTION_BLOCK;
    _eval_kill_data.action = ACTION_PROTECT;

    // Reset stats counters
    eval_reset_stats();
}
