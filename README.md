# C, Preprocessed with C

`cpc` is a preprocessing utility that introduces meta-blocks - blocks of code that are executed at compile time and output of which is inserted into the source code.

## Basic example
```c
// input.cm
#include <stdio.h>

int main() {
	// cpc introduces $$ meta-tokens.
	// Code between them will be put into its own file inside main,
	// executed, and replaced with its output.
	// This meta block expects a numeric argument, so provide it in meta-args.
	// E.g. 
	//    cpc 0_basic.cm cmb 69420
	int value = $$
		assert(argc == 2);
		printf("%s", argv[1]);
	$$;

	printf("value is %d\n", value);
}
```
Running the preprocessor:
```console
cpc input.cm -- -- 69420
```
Procudes `input.c`, that you then compile:
```console
cc input.c -o input
```
Running `./input` will print `69420`.

See other [examples](./examples).

## Usage
```console
cpc input.cm [-o output.c] <meta-compiler> <meta-args...>
```
* `<meta-compiler>` is the executable/command file that takes a path to C file and compiles it procuding executable with the same name. `cmb` is default implementation.
* `<meta-args...>` are passed to execution of meta-block.

## Building
This project uses [nob](https://github.com/tsoding/nob.h). Run this to bootstrap:
```console
cc nob.c -o nob
```
After that run `./nob` to build itself and `cpc`.