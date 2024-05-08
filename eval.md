# Using the `eval` toolkit

## The `EVAL_CATCH()` macro

The `EVAL_CATCH()` macro allows an arbitrary function to be called catching all (most?) situations that would lead the program to halt or block. Specifically:

1. Calling `exit()` inside the function will cause the function to stop and return execution after the macro;
2. `SIGSEGV`, `SIGBUS`, `SIGFPE` and `SIGILL` signals received while executing the function are caught;
3. The function will be stopped after a time set by the `_eval_env.timeout` variable.
4. The macro will also check if any files were left open by the test code. If so, an error message will be issued, and the file(s) will be closed.

The `_eval_env.stat` variable can be used to determine how the function was terminated:

+ A value of `0` indicates normal termination, i.e. the code reached the end without any interruption.
+ A value `> 0` indicates that the code was terminated by one of the reasons stated above. See below for more details on checking the reason for code termination.

The macro must be called as:

```C
EVAL_CATH( _code )
```

Where `_code` is any function or sequence of instructions that we wish to test. Here is a simple example:

```C
#include "eval.h"

int main() {

    char *p, a;

    printf("Test with p = -1...\n");
    eval_reset();
    EVAL_CATCH( p = (char *) -1; *p = 0; );
    printf("%s\n", _eval_env.stat ? "Test failed" : "Test ok" );

    printf("Test with p = &a...\n");
    eval_reset();
    EVAL_CATCH( p = &a; *p = 0; );
    printf("%s\n", _eval_env.stat ? "Test failed" : "Test ok" );
}
}
```

Compiling and running the code gives:

```bash
$ gcc -Wall -lm test-eval.c eval.c 
$ ./a.out 
Test with p = -1...
[✗] Segmentation fault (SIGSEGV)
Test failed
Test with p = &a...
Test ok
```

### Timeouts

The code being tested in the `EVAL_CATCH()` macro will need to be completed before `_eval_env.timeout` seconds have passed. Setting this variable to 0 will disable this behavior and the function will be allowed to run indefinitely.

The `eval_reset()` command will set the timeout value (`_eval_env.timeout`) to the compile time constant `EVAL_TIMEOUT` which defaults to 1.0s. You can change this value either at compile time by adding `-DEVAL_TIMEOUT=time` to the compiler options, or before calling the `EVAL_CATCH()` macro.

Note that in the case of a timeout the function will be terminated by a `SIGPROF` signal. For this reason, the user code is not allowed to use `SIGPROF`.

## The `EVAL_CATCH_IO()` macro

The `EVAL_CATH_IO()` macro works just like the `EVAL_CATH()` macro, but it also allows redirecting (standard) input and/or output from/to specific files. You should use this macro to test functions that will accept input from stdin and/or output to stdout.

The macro should be called as follows:

```C
EVAL_CATCH_IO( _code, _fstdin, _fstdout )
````

Where `_code` has the same behavior as in the `EVAL_CATCH()` macro and `_fstdin`/`_fstdout` are the names (strings) of the files to be used for `stdin` and `stdout`, respectively. If either are set to `NULL` then no redirection takes place.

Here is a very simple example:

```C
#include "eval.h"

int read_int() {
    int res;
    if ( scanf("%d", &res ) < 1 ) {
        fprintf(stderr,"Invalid integer\n");
        exit(1);
    };
    return res;
}

int main() {

    int res = 0;

    // Use "1234\n" as stdin
    FILE *f = fopen("test.stdin","w");
    fputs("1234", f );
    fclose( f );

    eval_reset();
    EVAL_CATCH_IO( res = read_int(); , "test.stdin", NULL )
    if ( _eval_env.stat ) {
        fprintf(stderr,"Test failed\n");
    } else {
        printf("Read value %d\n", res );
    }

    unlink( "test.stdin");
}
```

### Checking the reason for termination

As noted above, the `_eval_env.stat` will be different from 0 in case the code does not finish normally. This variable may assume one of the following values:

+ `0` - Success, the code completed normally
+ `EVAL_CATCH_EXIT` - The code was terminated by a call to the `exit()` function. You can check `_eval_exit_data.status` for the exit status value 
+ `EVAL_CATCH_ABORT` - The code was terminated by a call to the `abort()` function.
+ `EVAL_CATCH_BLOCKED` - The code was terminated due to a blocked function being called (this is the default behaviour when calling `execv()` for example)
+ `EVAL_CATCH_SIGNAL` - A signal was caught, usually indicating some error in the code (e.g. invalid pointer) or a timeout. You can check `_eval_env.signal` for the signal value

The toolkit provides a utility function `eval_termination()` to provide a description of how the test code terminated:

```C
#include "eval.h"

int main() {

    eval_reset();
    EVAL_CATCH( 
        exit(100);
    );
    printf("Test completed: %s\n", eval_termination() );
}
```

Compiling and running the code gives:

```test
$ gcc -lm test.c eval.c
$ ./a.out 
Test completed: exit(100) called
```

## Function Wrappers

Besides the `EVAL_CATCH*()` family of macros, the `eval` toolkit provides a set of function wrappers for multiple system functions that allow building simple tests to evaluate if these functions are being called correctly by the function being tested. In their simplest form, these macros allow capturing the arguments and return values used when calling these functions, so that these may be checked later against correct values. Additionally, these macros can be configured for other behaviors, such as returning error or success values without actually calling the underlying function, and counting how many times the function was called.

The specific behavior of each of the functions in the `eval` toolkit is controlled through specific `_eval_*_data` global variables that are described in the next section. The toolkit provides an `eval_reset()` function that resets all the functions to the default behavior, except for the `pause()`, `execl()`, `wait()`, `waitpid()`, `raise()`, and `kill()` functions. See the `eval_reset()` function for details.

### The `_eval_*_data` variables

For every function wrapped by the `eval` toolkit, there is an associated `_eval_func_data` global variable, where `func` is the name of the function that was wrapped. They all have the same general format:

```C
typedef struct {
    int action;         // User configurable function action
    int status;         // Number of times the function has been called
    
    return_type ret;    // Return value for the function (if the function is not void) 

    type_a param_a;     // Arguments used when calling the function, if any
    type_b param_b;
    ...

    extra fields;       // Additional optional fields to capture additional information
    ...
} _eval_func_type;
```

For example, the variable for the `int sleep( int seconds )` function is defined as:

```C
typedef struct {
    int action;
    int status;

    int ret;                // Return value

    unsigned int seconds;   // Function argument
} _eval_sleep_type;

extern _eval_sleep_type _eval_sleep_data;
```

#### `.action` field

The `.action` field is used to control how the function wrapper is supposed to behave:

+ The default behavior (`.action = EVAL_DEFAULT`) is to capture the function arguments, call the normal function, and store the return value (if any) in the `.ret` field.
+ Setting `.action` to `EVAL_BLOCK` will cause the test code to issue an error message and stop if the function is called.

Specific wrappers may define alternate behaviors (see the documentation for each function). For example, if the user calls the `sleep(5)` function with `_eval_sleep_data.action = 1`, the function will return immediately without calling `sleep()` and setting `_eval_sleep_data.seconds = 5` and `_eval_sleep_data.ret = 0`.

When creating new functions, or adding functionality to existing functions, keep in mind that you should always use `.action > EVAL_DEFAULT` to avoid conflicts with `EVAL_DEFAULT` and `EVAL_BLOCK`.

#### `.status` field

The `.status` field is used to count how many times a function is called. Each time a function is called, the corresponding `.status` field is incremented.

The only exception is the `exit()` function, which will be called at most once. For this function, the `.status` field will hold the exit status intended (i.e. the parameter used when calling `exit()`).

#### `.ret` field

If the function is not `void` then the variable will also include a `.ret` field of the appropriate type to store the return value of the function (if the actual function is called) or some other value depending on the `.action` field. 

Note that if a function is called multiple times only the last call will be kept. If you need to keep track of multiple function calls you will need to use the logging functionality (see for example the `signal()` function). 

#### Function parameters

If a function accepts any parameters (e.g. `int seconds` for the `sleep()` function) then the associated variable will also capture the values used when issuing the function call.

Again, as in the behavior of the `.ret` field, only the values for the last function call will be kept.

## Reporting test errors

When evaluating the implementation of some specific function, we will typically call the function inside an `EVAL_CATCH()` macro several times with different combinations of parameters and other options (e.g. missing files). If the function does not behave as expected, an error should be logged using the `eval_error()` command, which uses the same syntax as the `printf()` function, but also increments the `_eval_stats.error` variable (see below for more details on the `eval_error()` function).

Once all tests have been run, you should call the `eval_complete(char msg[])` function. If all tests worked (i.e. `_eval_stats.error` is 0) the routine will print out a success message, otherwise it will print an error message indicating how many errors were found. See below for more details on the `eval_complete()` function.

Here is a simple example:

```C
...

eval_reset(); // This also resets _eval_stats.error

// Test with paramA

... // Set test conditions

EVAL_CATCH( test_function( paramA ) );
if ( ! proper_behavior_A )
    eval_error("Testing test_function( paramA ) failed" );

// Test with paramB

... // Set test conditions

EVAL_CATCH( test_function( paramB ) );
if ( ! proper_behavior_B )
    eval_error("Testing test_function( paramB ) failed" );

...

// nerr has the total number of errors
nerr += eval_complete("test_function()");

```

## Logs

There are some specific tests where a given function must be called several times with different parameters. Since the `_eval_*_data` variables will only store information regarding the last time a function was called, the toolkit adds a simple logging functionality that can be used to test for these situations.

The toolkit makes available 3 logs (success, error and data). The first two (success and error) are generally used by the functions being tested for issuing success and error messages, and the last (data) is generally used by the wrapper functions to log the function name, calling parameters and/or return values.

### Clearing the logs

If you want to clear a specific log you can use the `initlog()` function with the desired log variable (`_data_log`, `_error_log` or `_success_log`).

```C
    // Clear the data log
    initlog(&_data_log);
```

See also the utility function `eval_clear_logs()` below. 

### Writing to the logs

A log entry can be written by using one of `datalog()`, `errorlog()` or `successlog()` functions, which will write to one of the `_data_log`, `_error_log` or `_success_log` logs, respectively. The syntax is the same as for the `printf()` family of functions.

Each call will add a new entry to the log, and these messages will not be echoed to `stdout` or `stderr`. Before adding a new entry, the log is checked for space (see the `LOGSIZE` macro in `eval.h`) and an error is issued should the log buffer overflow, stopping the test.

Example:

```C
    // Log an error
    errorlog("Unable to open file %s", filename);
```

### Viewing/searching log contents

To view the content of a specific log the `printlog( log )` function may be used:

```C
    // Print the contents of the data log
    printlog(&_data_log);
```

However, this is not usually done except for debugging tests. It is more common to search for specific log contents using `findinlog( log, val, ... )`. If the search string is found, the routine will return the corresponding log entry index. Otherwise, the routine will return -1. The routine accepts `printf()` style arguments:

```C
    // look for "a=123" in success_log
    if ( 0 > findinlog( &_success_log, "a=%d", 123 ) ) {
        printf("Found!\n");
    } else {
        printf("It's not there.\n");
    }
```

To simplify the processing of messages that should appear sequentially, the library also provides the `rmheadmsg( log, msg )` function. The function serves a dual purpose:

+ It checks if the top (head) line in the log contains `msg`
+ If so, this line is deleted from the log

The function will return 0 if `msg` was found and -1 otherwise:

```C
    // Expects success log to read
    // 1. "message 1"
    // 2. "message 2 - extra"

    int err = 0;
    err |= rmheadmsg( &_success_log, "message 1" );
    err |= rmheadmsg( &_success_log, "message 2" ); // notice the missing " - extra"

    if ( nerr ) printf("Invalid log.\n");
```

### Utility functions

#### eval_check_successlog() / eval_check_errorlog()

These functions will check the top of the corresponding log for a specific message (using `rmheadmsg()`):

+ On success, they return 1 and print the complete head log entry. The head log entry is removed;
+ On failure, they return 0 and print the expected message and the actual head log entry.

The functions have a syntax similar to the `printf()` family of functions:

```C
    // look for "b=234" in the head error_log entry
    if ( eval_check_errorlog("b=%d", 234) ) ncorrect++;
```

#### eval_clear_logs()

This function will clear all three logs by calling `initlog()` for each of them.

#### eval_close_logs()

This function will check for any remaining messages on the success and error logs and print them. Before terminating, it also clears the logs.

## Wrapped functions

With only a few exceptions (e.g. the `exit()` function) all of the functions in the eval toolkit have as a default behavior:

1. Capturing the parameters of the function call in `_eval_*_data` elements of the same name;
2. Calling the underlying function;
3. Capturing the return value and storing it in `_eval_*_data.ret` (if the function is not `void`).

The `_eval_*_data.status` field will be incremented by 1.

The behavior of a function can be changed by setting the corresponding `_eval_*_data.action` value. Setting this value to `EVAL_BLOCK` will stop the evaluation if the corresponding function is called by the test code, and an error message is issued. Other values triggering different behaviors are described below.

### `exit( int status )`

The `exit()` function is a special case of the `eval` toolkit given that the underlying function will never be called. Instead, the function will be terminated and execution returned to the `EVAL_CATCH()` macro.

Function options:

+ `.action = 1` - Print info message with exit status

Fields in `_eval_exit_data`:

+ `.status` - Function exit status, i.e., `exit()` function parameter

### `abort()`

Like the `exit()` function, the `abort()` function will never be called. Instead, the function will be terminated and execution returned to the `EVAL_CATCH()` macro.

Function options:

+ `.action = 1` - Print info message

### `sleep( unsigned int seconds )`

Function options:

+ `.action = 1` - Return 0 immediately without calling `sleep()`

Fields in `_eval_sleep_data`:

+ `.ret`     - Return value of the function
+ `.seconds` - Value of the `seconds` parameter

### `fork( )`

Function options:

+ `.action = 3` - Return -1 immediately without calling `fork()` (i.e. issue error)
+ `.action = 2` - Return 1 immediately without calling `fork()` (i.e. behave as parent process)
+ `.action = 1` - Return 0 immediately without calling `fork()` (i.e. behave as child process)

Fields in `_eval_fork_data`:

+ `.ret`     - Return value of the function

### `wait(int *stat_loc)`

__Note__: The wrapper does not modify `*stat_loc` which means that the use of the `WIFEXITED()`, `WEXITSTATUS()`, etc. macros are not supported.

Function options:

+ `.action = 3` - (inject) Return immediately the value the previously set value in `_eval_wait_data.set`
+ `.action = 2` - (error) Return -1
+ `.action = 1` - (success) Return 0 immediately without calling `wait()`

Fields in `_eval_wait_data`:

+ `.ret`      - Return value of the function
+ `.stat_loc` - Value of the `stat_loc` parameter

### `waitpid( pid_t pid, int *stat_loc, int options )`

__Note__: The wrapper does not modify `*stat_loc` which means that the use of the `WIFEXITED()`, `WEXITSTATUS()`, etc. macros are not supported.

Function options:

+ `.action = 2` - (error) Return -1 (error) immediately without calling `waitpid()`, and sets errno to EINVAL
+ `.action = 1` - (success) Return `pid` immediately without calling `waitpid()`

Fields in `_eval_waitpid_data`:

+ `.ret`      - Return value of the function
+ `.pid`      - Value of the `pid` parameter
+ `.stat_loc` - Value of the `stat_loc` parameter
+ `.options`  - Value of the `options` parameter

### `kill(pid_t pid, int sig)`

Function options:

+ `.action = 3` - (log) Log call parameters in `datalog` and return 0 without calling `kill()`
+ `.action = 2` - (error) Return -1 immediately without calling `kill()`
+ `.action = 1` - (success) Return 0 immediately without calling `kill()`
+ `.action = EVAL_PROTECT` - Prevent routine from signaling self, parent, process group and every process belonging to process owner

Fields in `_eval_kill_data`:

+ `.ret`      - Return value of the function
+ `.pid`      - Value of the `pid` parameter
+ `.sig`      - Value of the `sig` parameter

### `raise( int sig )`

Function options:

+ `.action = 2` - (error) Issue error and return 0 without calling `raise()`
+ `.action = 1` - (success) Return 0 immediately without calling `raise()`
+ `.action = EVAL_PROTECT` - Issue an error message and return 0 without calling `raise()`

Fields in `_eval_raise_data`:

+ `.ret`      - Return value of the function
+ `.sig`      - Value of the `sig` parameter

### `signal(int signum, sighandler_t handler )`

The function will prevent arming the `SIGPROF` signal, which is used by the `EVAL_CATCH()` macros to implement timeouts. Should the user try to arm `SIGPROF` the routine will issue an error, returning `SIG_ERR` and setting `errno = EINVAL`.

Function options:

+ `.action = 3` - (log) Log call parameters in `datalog` and return `SIG_DFL` without calling `signal()`
+ `.action = 2` - (error) Return `SIG_ERR` without calling `signal()`
+ `.action = 1` - (success) Return `SIG_DFL` without calling `signal()`

Fields in `_eval_signal_data`:

+ `.ret`      - Return value of the function
+ `.signum`   - Value of the `signum` parameter
+ `.handler`  - Value of the `handler` parameter

### `sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)`

The function will prevent arming the `SIGPROF` signal, which is used by the `EVAL_CATCH()` macros to implement timeouts. Should the user try to arm `SIGPROF` the routine will issue an error, returning -1 and setting `errno = EINVAL`.

Function options:

+ `.action = 3` - (log) Log call parameters in `datalog` and return 0 without calling `sigaction()`. Note: unless `act -> sa_flags` had the `SA_SIGINFO` bit set, the function will be logged as if the corresponding `signal()` call was made.
+ `.action = 2` - (error) Return -1 without calling `sigaction()` 
+ `.action = 1` - (success) Return 0 without calling `sigaction()`

Fields in `_eval_sigaction_data`:

+ `.ret`      - Return value of the function
+ `.signum`   - Value of the `signum` parameter
+ `.act`      - Value of the `act` parameter
+ `.oldact`   - Value of the `oldact` parameter

### `pause()`

Function options:

+ `.action = 1` - Return -1 without calling `pause()`. Note: the `pause()` command always returns -1,

Fields in `_eval_pause_data`:

+ `.ret`      - Return value of the function

### `alarm( unsigned int seconds )`

Function options:

+ `.action = 2` - (time left) Return 1 without calling `alarm()`
+ `.action = 1` - (success) Return 0 without calling `alarm()`

Fields in `_eval_alarm_data`:

+ `.ret`      - Return value of the function
+ `.seconds`  - Value of the `seconds` parameter

### `msgget(key_t key, int msgflg)`

Function options:

+ `.action = 4` - Return error on 1st time called, `_eval_msgget_data.msqid` on 2nd time
+ `.action = 3` - (inject on create only) Return error if `IPC_CREAT` was not specified, `_eval_msgget_data.msqid` otherwise
+ `.action = 2` - (error) Return -1
+ `.action = 1` - (inject) Return the value set in `_eval_msgget_data.msqid`

Fields in `_eval_msgget_data`:

+ `.ret`      - Return value of the function
+ `.key`      - Value of the `key` parameter
+ `.msgflg`   - Value of the `msgflg` parameter

### `msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)`

Function options:

+ `.action = 4` - (ignore) Return 0 without calling `msgsnd()`
+ `.action = 3` - (inject) Call `msgsnd()` but using the parameters specified in `_eval_msgsnd_data`
+ `.action = 2` - (error) Return -1
+ `.action = 1` - (capture) Return `msgsz` (success). If `msgp` was a valid pointer, then the data in `*msgp` is copied to a newly allocated buffer at `_eval_msgsnd_data.msgp`. This buffer __must__ be freed afterwards.

Fields in `_eval_msgsnd_data`:

+ `.ret`      - Return value of the function
+ `.msqid`    - Value of the `msqid` parameter
+ `.msgp`     - Value of the `msgp` parameter
+ `.msgsz`    - Value of the `msgsz` parameter
+ `.msgflg`   - Value of the `msgflg` parameter

### `msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)`

Function options:

+ `.action = 3` - (inject) Copy data in `*_eval_msgsnd_data.msgp` to `*msgp`. The pointer `msgp` is checked before copying.
+ `.action = 2` - (error) Return -1
+ `.action = 1` - (capture) Return `msgsz` (success).

Fields in `_eval_msgrcv_data`:

+ `.ret`      - Return value of the function
+ `.msqid`    - Value of the `msqid` parameter
+ `.msgp`     - Value of the `msgp` parameter
+ `.msgtyp`   - Value of the `msgtyp` parameter
+ `.msgsz`    - Value of the `msgsz` parameter
+ `.msgflg`   - Value of the `msgflg` parameter

### `msgctl(int msqid, int cmd, struct msqid_ds *buf)`

Function options:

+ `.action = 1` - (success) Return 0

Fields in `_eval_msgctl_data`:

+ `.ret`      - Return value of the function
+ `.msqid`    - Value of the `msqid` parameter
+ `.cmd`      - Value of the `cmd` parameter
+ `.buf`      - Value of the `buf` parameter

### `semget(key_t key, int nsems, int semflg)`

Function options:

+ `.action = 4` - Return error on 1st time called, `_eval_semget_data.semid` on 2nd time
+ `.action = 3` - (inject on create only) Return error if `IPC_CREAT` was not specified, `_eval_semget_data.semid` otherwise
+ `.action = 2` - (error) Return -1 (error)
+ `.action = 1` - (inject) Return the value set in `_eval_semget_data.semid`

Fields in `_eval_semget_data`:

+ `.ret`      - Return value of the function
+ `.nsems`    - Value of the `nsems` parameter
+ `.semflg`   - Value of the `semflg` parameter

### `semctl(int semid, int semnum, int cmd, ... )`

Function options:

+ `.action = 3` - (log) Log function call ("semctl") in the data log and return 0
+ `.action = 2` - (error) return -1
+ `.action = 1` - (success) Return 0

Fields in `_eval_semctl_data`:

+ `.ret`      - Return value of the function
+ `.semid`    - Value of the `semid` parameter
+ `.semnum`   - Value of the `semnum` parameter
+ `.cmd`      - Value of the `cmd` parameter
+ `.arg`      - Argument list for commands with extended argument parameters (e.g. `IPC_SET`)

### `semop(int semid, struct sembuf *sops, size_t nsops)`

Function options:

+ `.action = 3` - (log) Log function call ("semop") in the data log and return 0
+ `.action = 2` - (error) return -1
+ `.action = 1` - (success) Return 0

Fields in `_eval_semop_data`:

+ `.ret`      - Return value of the function
+ `.semid`    - Value of the `semid` parameter
+ `.sops`     - Value of the `sops` parameter
+ `.nsops`    - Value of the `nsops` parameter

### `shmget(key_t key, size_t size, int shmflg)`

Function options:

+ `.action = 4` - Return error on 1st time called, `_eval_shmget_data.shmid` on 2nd time
+ `.action = 3` - (inject on create only) Return error if `IPC_CREAT` was not specified, `_eval_shmget_data.shmid` otherwise
+ `.action = 2` - (error) Return -1 (error)
+ `.action = 1` - (inject) Return the value set in `_eval_shmget_data.shmid`

Fields in `_eval_shmget_data`:

+ `.ret`      - Return value of the function
+ `.size`     - Value of the `size` parameter
+ `.shmflg`   - Value of the `shmflg` parameter

### `shmat( int shmid, const void *shmaddr, int shmflg)`

Function options:

+ `.action = 1` - (inject) Return the value set in `_eval_shmat_data.shmaddr`

Fields in `_eval_shmat_data`:

+ `.ret`      - Return value of the function
+ `.shmid`    - Value of the `shmid` parameter
+ `.shmaddr`  - Value of the `shmaddr` parameter
+ `.shmflg`   - Value of the `shmflg` parameter

### `shmdt(const void *shmaddr)`

Function options:

+ `.action = 2` - (error) Return -1
+ `.action = 1` - (success) Return 0

Fields in `_eval_shmdt_data`:

+ `.ret`      - Return value of the function
+ `.shmaddr`  - Value of the `shmaddr` parameter

### `shmctl(int shmid, int cmd, struct shmid_ds *buf)`

Function options:

+ `.action = 3` - (log) Log function call ("shmctl") in the data log and return 0
+ `.action = 2` - (error) return -1
+ `.action = 1` - (success) Return 0

Fields in `_eval_semctl_data`:

+ `.ret`      - Return value of the function
+ `.semid`    - Value of the `semid` parameter
+ `.cmd`      - Value of the `cmd` parameter
+ `.buf`      - Value of the `buf` parameter

### `mkfifo(const char *path, mode_t mode)`

Function options:

+ `.action = 2` - (error) Return -1 without calling `mkfifo()`
+ `.action = 1` - (success) Return 0 without calling `mkfifo()`

Fields in `_eval_mkfifo_data`:

+ `.ret`      - Return value of the function
+ `.path`     - Copy of the value of the `path` parameter
+ `.mode`     - Value of the `mode` parameter

### `S_ISFIFO(mode)`

Function options:

+ `.action = 2` - Return 0 without calling `S_ISFIFO()`
+ `.action = 1` - Return 1 without calling `S_ISFIFO()`

Fields in `_eval_isfifo_data`:

+ `.ret`      - Return value of the macro
+ `.mode`     - Value of the `mode` parameter

### `remove(const char * path)`

Function options:

+ `.action = 3` - (log) Log call parameters and return 0 without calling `remove()`. Note that the function call is logged as if the corresponding `unlink()` function call was made.
+ `.action = 2` - (error) Return -1 without calling `remove()`
+ `.action = 1` - (success) Return 0 without calling `remove()`

Fields in `_eval_remove_data`:

+ `.ret`      - Return value of the function
+ `.path`     - Copy of the value of the `path` parameter

### `unlink(const char * path)`

Function options:

+ `.action = 3` - (log) Log call parameters and return 0 without calling `unlink()`
+ `.action = 2` - (error) Return -1 without calling `unlink()`
+ `.action = 1` - (success) Return 0 without calling `unlink()`

Fields in `_eval_unlink_data`:

+ `.ret`      - Return value of the function
+ `.path`     - Copy of the value of the `path` parameter

### `atoi(const char *nptr )`

Since `atoi()` is unsafe, the routine will first check for a valid string. If the supplied string is NULL or is longer than 32 characters the routine will issue an error message and return -1.

Function options:

+ `.action = 1` - Return 1 as the result without calling `atoi()`

Fields in `_eval_atoi_data`:

+ `.ret`      - Return value of the function
+ `.nptr`     - Copy of the value of the `nptr` parameter

### `fclose( FILE* stream )`

The routine will first check for a valid pointer. On error, the routine will issue an error message and return -1.

Function options:

+ `.action = 2` - (error) Return error (EOF) and set `errno` to `EINVAL`
+ `.action = 1` - (success) Return 0

Fields in `_eval_fclose_data`:

+ `.ret`      - Return value of the function

### `fread( void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream )`

The routine will validate the parameters (valid pointers, etc.) before calling `fread()`. On error, the function will return 0 without calling `fread()`.

Function options:

+ `.action = 2` - (error) Return error (0) and set `errno` to `EINVAL`
+ `.action = 1` - (success) Return `nmemb`

Fields in `_eval_fread_data`:

+ `.ret`      - Return value of the function
+ `.ptr`      - Value of the `ptr` parameter
+ `.size`     - Value of the `size` parameter
+ `.nmemb`    - Value of the `nmemb` parameter
+ `.stream`   - Value of the `stream` parameter

### `fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)`

The routine will validate the parameters (valid pointers, etc.) before calling `fwrite()`. On error, the function will return 0 without calling `fwrite()`.

Function options:

+ `.action = 2` - (error) Return error (0) and set `errno` to `EINVAL`
+ `.action = 1` - (success) Return `nmemb`

Fields in `_eval_fwrite_data`:

+ `.ret`      - Return value of the function
+ `.ptr`      - Value of the `ptr` parameter
+ `.size`     - Value of the `size` parameter
+ `.nmemb`    - Value of the `nmemb` parameter
+ `.stream`   - Value of the `stream` parameter

### `fseek(FILE *stream, long offset, int whence)`

The routine will first check for a valid pointer and valid values for the `whence` parameter. On error, the routine will issue an error message and return -1.

Function options:

+ `.action = 2` - (error) Return error (-1) and set `errno` to `EINVAL`
+ `.action = 1` - (success) Return 0

Fields in `_eval_fseek_data`:

+ `.stream`   - Value of the `stream` parameter
+ `.offset`   - Value of the `offset` parameter
+ `.whence`   - Value of the `whence` parameter

### `execl(const char *path, ... )``

The default settings implemented by the `eval_reset()` function will prevent the test function from calling this function, instead issuing an error and terminating the function.

The function can be made to call `execl()` by setting `.action = 0` but unless the call fails this will replace the current process with a new one. Unless a new process was forked before this happens, this will of course stop the tests.

Function options:

+ `.action = 1` - Return -1 without calling `execl()`

Fields in `_eval_unlink_data`:

+ `.ret`      - Return value of the function. Note that this will only be updated in case of failure.

## Additional functions

### `eval_reset()`

The `eval_reset()` function resets all functions to the default behavior and clears all counters. Specifically:

+ All `.action` and `.status` variables are set to 0, as are all remaining fields of the `_eval_*_data` variables
+ `_eval_stats.error` and `_eval_stats.info` are set to 0

Additionally, the function will:

+ Set the timeout value to the `EVAL_TIMEOUT` macro. To disable timeouts you can compile the code with `-DEVAL_TIMEOUT=0`
+ Block the execution of `pause()` and `execl()` by setting their `.action` values to `EVAL_BLOCK`, as these would stop or destroy the current test.
+ Block the execution of `wait()` and `waitpid()` by setting their `.action` values to `EVAL_BLOCK`, as these would halt the current test.
+ Prevent signals to self by setting the `raise()` and `kill()` by setting their `.action` values to `EVAL_PROTECT`, to avoid stopping the test.

Note that you can change all of these behaviors by modifying the corresponding `_eval_*_data` variable before calling the `EVAL_CATCH()` macro.

### `eval_checkptr()`

The `eval_checkptr()` checks if a pointer is valid by reading and writing 1 byte from/to the specified address. The syntax is as follows:

```C
int eval_checkptr( void* ptr )
````

The function will return one of the following values:

+ 0 - pointer is valid
+ 1 - NULL pointer
+ 2 - (-1) pointer
+ 3 - Segmentation fault when accessing pointer
+ 4 - Bus Error when accessing pointer
+ 5 - Invalid signal caught (should never happen)

### `eval_error()`

Prints out an information message. It precedes the message with a red "[✗]" marker. The `_eval_stats.error` variable is incremented.

The syntax is the same as the `printf()` function:

```C
int eval_error(const char *restrict format, ...)
```

The function returns the updated value of `_eval_stats.error`

### `eval_info()`

Prints out an information message. It precedes the message with a blue "[ℹ︎]" marker. The `_eval_stats.info` variable is incremented.

The syntax is the same as the `printf()` function:

```C
int eval_info(const char *restrict format, ...)
```

The function returns the updated value of `_eval_stats.info`

### `eval_success()`

Prints out a success message. It precedes the message with a green "[✔]" marker. The `_eval_stats.info` variable is incremented.

The syntax is the same as the `printf()` function:

```C
int eval_success(const char *restrict format, ...)
```

The function returns the updated value of `_eval_stats.info`

### `create_lockfile()` / `remove_lockfile()`

These functions allow creating/removing a "locked" file, i.e., a file with `000` permissions. The syntaxes are as follows:

```C
int create_lockfile( const char * filename );
int remove_lockfile( const char * filename );
```

These are usually used to test the detection of write or read errors. The file is created with 4 bytes and the characters `LOCK`.

Please note that the file __can still be removed by `unlink()` or `remove()`__. The `remove_lockfile()` will simply check if the file still exists before removing it.

Both functions return 0 on success, and -1 on error.

__Note__: When possible, it is recommended to use instead the `/etc/sudoers` file as a "locked" file, since it is available on all test systems, and is only readable by the `root` user. Use these functions only when a specific file name is required.

## Compilation

Just include `eval.h` and compile the code together with `eval.c`. Depending on your system, you may need to include the math library (`-lm`), e.g.:

```bash
gcc -Wall -pedantic -lm test.c eval.c
```

The extra warning flags (`-Wall` and `-pedantic`) are (obviously) not needed, but the library will not issue any compiler warnings even when compiled with these flags.

Please note that some of the functions used, while being strictly POSIX compliant, are not standard C compliant. To compile enforcing C99 or C11 compliance, you may need to define `_XOPEN_SOURCE` and `_POSIX_C_SOURCE=200809L` (or higher).

### Linux

The following compiles with no warnings with gcc 6.3.0 on x86_64 Linux:

```bash
gcc -Wall -pedantic -lm -std=c11 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE test.c eval.c 
```

If you don't define `_XOPEN_SOURCE` you will get a warning from the `sys/ipc.h` file; not defining `_POSIX_C_SOURCE` to at least `200809L` will cause errors with `sigaction()` calls (missing `siginfo_t`, no `sa_sigaction` field in `sigaction` structure, etc.), `strsignal`, `SA_RESTART`, and `strnlen()`.

### Mac OS X

On Mac OS X these defines are not necessary. The following compiles with no warnings with clang 15.0 on Mac OS X (14.4.1):

```bash
clang -std=c11 -Wall -pedantic test.c eval.c
```
