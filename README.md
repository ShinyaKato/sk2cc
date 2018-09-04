# sk2cc

Simple C compiler developed by @ShinyaKato

## build

Run make command, then `sk2cc` is generated.

```
$ make sk2cc
```

## compilation

Compiler `sk2cc` recieves path to a source file and generates assembly to stdout.
To generate executable, use gcc.

```
$ ./sk2cc examples/queen.c > queen.s
$ gcc queen.s -o queen
$ ./queen
```

## example

See example programs that can be compiled with `sk2cc` in [examples](https://github.com/ShinyaKato/sk2cc/tree/master/examples).
