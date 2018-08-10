# seccamp2018

Simple C compiler developed by @ShinyaKato

## build

Run make command, then `cc` is generated.

```
$ make cc
```

## compilation

Compiler `cc` reads C program from stdin and generates assembly to stdout.
To generate executable, use gcc with `-no-pie` option.

```
$ cat examples/queen.c | ./cc > queen.s
$ gcc -no-pie queen.s
```

## example

See example programs that can be compiled with `cc` in [examples](https://github.com/ShinyaKato/seccamp2018/tree/master/examples).
