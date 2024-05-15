# eval project

The `eval` project offers a simple toolkit for evaluating student C code in an academic environment. It is meant to be used by the teaching staff to prepare a set of unit tests for student code in the framework of a specific project/assignment, that the students can then use to test their code, as well as for grading purposes.

The main features of the `eval` environment are:

+ Providing a sandboxed environment for running student code that will catch all (most?) situations that would lead the program to halt or block, giving detailed information when this happens;
+ Redirecting input/output to automate tests requiring interactive use;
+ Capturing system function call parameters and outputs, and allowing the test to be run using specific behaviors for these functions.
+ Providing a simple log system, useable with functions with a syntax similar to the `printf()` family, that can be used for output by the test code and later easily verified.

For more details on the features available in the toolkit be sure to check the [documentation](eval.md).

The toolkit was initially developed for use in the "Operating Systems" course unit taught at [ISCTE - University Insitute of Lisbon](https://www.iscte-iul.pt/school/8/escola-de-tecnologias-arquitetura), Portugal, for undergraduate computer science students.

## Documentation

Full documentation, including compilation instructions, is available [here](eval.md). The source code is also fully documented.
