# multicorn

![multicorn](https://i.redd.it/ipomv2xs5ew41.jpg)

Multicorn is an experimental multi-interpreter server/framework for Python.

## Motivation

For a long time, Python programmers were suffered from Python's GIL issue when building server-side applications. To fully use the multiple CPU cores on modern machines, we have to use multi-processing since one python interpreter instance can only use one CPU core because of the GIL.

This caused huge memory costs and made stateful server programming harder because we can't share data between processes easily.

[PEP 734](https://peps.python.org/pep-0734/) brings the new hope: it adds the `concurrent.interpreters` module to the standard library (Python 3.14), so we can have multiple python interpreters in one process. Combined with the per-interpreter GIL from [PEP 684](https://peps.python.org/pep-0684/) (Python 3.12), every interpreter has its own GIL and all the interpreters can run in parallel.

Multicorn is an experimental project for writing server-side network applications in multi-interpreters. It's more like a multi-interpreter version of [gunicorn](https://gunicorn.org/), instead of using multi-process.

## Usage

Write a WSGI application as usual:

```python
# myapp.py
def app(environ, start_response):
    start_response("200 OK", [("Content-Type", "text/plain")])
    return [b"Hello from multicorn!\n"]
```

Then serve it with multiple interpreters, each running in its own thread and
sharing the listening socket:

```python
import multicorn

server = multicorn.Server(
    "multicorn.workers:WSGIWorker",
    ["myapp:app"],
    workers_count=4,
)
server.run(("localhost", 3000))
```

Now `curl http://localhost:3000/` is served by one of the four sub-interpreters.

Async (ASGI) applications are supported as well:

```python
# myasgi.py
async def app(scope, receive, send):
    assert scope["type"] == "http"
    await send({
        "type": "http.response.start",
        "status": 200,
        "headers": [(b"content-type", b"text/plain")],
    })
    await send({
        "type": "http.response.body",
        "body": b"Hello from multicorn!\n",
    })
```

```python
import multicorn

server = multicorn.Server(
    "multicorn.workers:ASGIWorker",
    ["myasgi:app"],
    workers_count=4,
)
server.run(("localhost", 3000))
```

The ASGI worker speaks HTTP/1.1 and supports the optional
[lifespan](https://asgi.readthedocs.io/en/latest/specs/lifespan.html) protocol;
apps that don't implement lifespan keep working unchanged.

## Current status

- [x] Support run WSGI server in multi-interpreters.
- [x] Support run ASGI server in multi-interpreters.
- [ ] Manage the life cycle of sub-interpreters.

## Limitations

Multicorn uses the `concurrent.interpreters` module from [PEP 734](https://peps.python.org/pep-0734/). On Python 3.14 or later this module is available in the standard library; on earlier versions it is provided by the [backports.interpreters](https://github.com/aisk/backports.interpreters) package, so Python 3.8 or later is required.

True multi-core parallelism relies on the per-interpreter GIL introduced in [PEP 684](https://peps.python.org/pep-0684/), which is only available on Python 3.12 and later. On Python 3.11 or earlier all sub-interpreters share a single GIL, so multicorn runs but can not use multiple CPU cores in one process.

## License

Multicore is distributed by a [MIT license](https://github.com/aisk/multicorn/tree/master/LICENSE).
