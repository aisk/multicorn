# Ngru

Ngru is a python wsgi server writen in C. Using libevent's http library.

## Build

on ubuntu

```
> sudo apt-get install python3.6-dev
> sudo apt-get install libevent-dev
> make
> ./ngru
```

## Use

The wsgi function name and module is hard code in ngru.c, you can change it and recompile ngru.
