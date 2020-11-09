# multicorn

Multicorn is an experimental multi-interpreter server/framework for Python.

## Motivation

For a long time, Python programmers were suffered from Python's GIL issue when building server-side applications. To fully use the multiple CPU cores on modern machines, we have to use multi-processing since one python interpreter instance can only use one CPU core because of the GIL.

This caused huge memory costs and made stateful server programming harder because we can't share data between processes easily.

[PEP554](https://www.python.org/dev/peps/pep-0554/) brings the new hope: we can have multiple python interpreters in one process, and every interpreter has its own GIL. All the interpreters can run parallel.

Multicorn is an experimental project for writing server-side network applications in multi-interpreters. It's more like a multi-interpreter version of [gunicorn]https://gunicorn.org/), instead of using multi-process.

## Current status

- [x] Support run TCP server in multi-interpreters.
- [x] Support run WSGI server in multi-interpreters.
- [ ] Support run ASGI server in multi-interpreters.
- [ ] Manage the life cycle of sub-interpreters.

## Limitations

Multicorn is using private modules which are added in python3.8, so python3.8 or later is required.

For the current implementation of the sub-interpreter in python, the GIL is shared between all sub-interpreters. So multicorn can not use multi CPU cores in one process now sadly. There is a project to solve it: [multi-core-python](https://github.com/ericsnowcurrently/multi-core-python/), let's keep tracking on it.

## License

Multicore is distributed by a [MIT license](https://github.com/aisk/multicorn/tree/master/LICENSE).
