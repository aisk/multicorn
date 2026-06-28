import asyncio
import importlib
import socket
import urllib.parse
from http import HTTPStatus
from wsgiref import simple_server


class EchoWorker:
    def run(self, bind, fd, family):
        sock = socket.fromfd(fd, family, socket.SOCK_STREAM)
        while True:
            client, address = sock.accept()
            with client:
                while True:
                    data = client.recv(1024)
                    if not data:
                        break
                    client.sendall(data)


class WSGIWorker:
    def __init__(self, app):
        module_name, sep, attr_name = app.partition(":")
        if not sep or not module_name or not attr_name:
            raise ValueError(
                f"app must be 'module' and 'attr' joined by a colon, got {app!r}"
            )

        module = importlib.import_module(module_name)
        self.app = getattr(module, attr_name)

    def run(self, bind, fd, family):
        # Don't let WSGIServer create, bind and listen on its own socket; reuse
        # the listening fd shared from the parent interpreter instead.
        server = simple_server.WSGIServer(
            bind, simple_server.WSGIRequestHandler, bind_and_activate=False
        )
        server.socket.close()
        server.socket = socket.fromfd(fd, family, socket.SOCK_STREAM)
        host, port = bind[0], bind[1]
        server.server_name = socket.getfqdn(host)
        server.server_port = port
        server.setup_environ()
        server.set_app(self.app)
        server.serve_forever()


class ASGIWorker:
    def __init__(self, app):
        module_name, sep, attr_name = app.partition(":")
        if not sep or not module_name or not attr_name:
            raise ValueError(
                f"app must be 'module' and 'attr' joined by a colon, got {app!r}"
            )

        module = importlib.import_module(module_name)
        self.app = getattr(module, attr_name)

    def run(self, bind, fd, family):
        self.host, self.port = bind[0], bind[1]
        # Reuse the listening fd shared from the parent interpreter instead of
        # binding a new socket.
        sock = socket.fromfd(fd, family, socket.SOCK_STREAM)
        asyncio.run(self._serve(sock))

    async def _serve(self, sock):
        await self._lifespan_startup()
        server = await asyncio.start_server(self._handle, sock=sock)
        async with server:
            await server.serve_forever()

    async def _lifespan_startup(self):
        # The lifespan protocol is optional: apps that don't speak it raise on
        # the lifespan scope, which we treat as "unsupported" and ignore.
        receive_queue: "asyncio.Queue" = asyncio.Queue()
        send_queue: "asyncio.Queue" = asyncio.Queue()
        scope = {"type": "lifespan", "asgi": {"version": "3.0", "spec_version": "2.0"}}

        async def lifespan():
            try:
                await self.app(scope, receive_queue.get, send_queue.put)
            except BaseException:
                await send_queue.put(None)

        task = asyncio.ensure_future(lifespan())
        await receive_queue.put({"type": "lifespan.startup"})
        message = await send_queue.get()
        if message is None:
            await asyncio.gather(task, return_exceptions=True)
            return
        if message.get("type") == "lifespan.startup.failed":
            raise RuntimeError(message.get("message", "ASGI lifespan startup failed"))
        # Startup succeeded; keep the task alive for the server's lifetime.
        self._lifespan_task = task

    async def _handle(self, reader, writer):
        try:
            request_line = await reader.readline()
            if not request_line:
                return
            try:
                method, target, version = (
                    request_line.decode("latin-1").rstrip("\r\n").split(" ", 2)
                )
            except ValueError:
                return

            headers = []
            content_length = 0
            while True:
                line = await reader.readline()
                if line in (b"\r\n", b"\n", b""):
                    break
                name, _, value = line.partition(b":")
                name = name.strip().lower()
                value = value.strip()
                headers.append((name, value))
                if name == b"content-length":
                    try:
                        content_length = int(value)
                    except ValueError:
                        content_length = 0

            path, _, query_string = target.partition("?")
            scope = {
                "type": "http",
                "asgi": {"version": "3.0", "spec_version": "2.3"},
                "http_version": version.partition("/")[2] or "1.1",
                "method": method,
                "scheme": "http",
                "path": urllib.parse.unquote(path),
                "raw_path": path.encode("latin-1"),
                "query_string": query_string.encode("latin-1"),
                "root_path": "",
                "headers": headers,
                "server": (self.host, self.port),
                "client": writer.get_extra_info("peername"),
            }

            remaining = content_length

            async def receive():
                nonlocal remaining
                if remaining <= 0:
                    return {"type": "http.request", "body": b"", "more_body": False}
                chunk = await reader.read(min(remaining, 65536))
                if not chunk:
                    remaining = 0
                    return {"type": "http.disconnect"}
                remaining -= len(chunk)
                return {
                    "type": "http.request",
                    "body": chunk,
                    "more_body": remaining > 0,
                }

            response_started = False

            async def send(event):
                nonlocal response_started
                if event["type"] == "http.response.start":
                    status = event["status"]
                    try:
                        phrase = HTTPStatus(status).phrase
                    except ValueError:
                        phrase = ""
                    out = [f"HTTP/1.1 {status} {phrase}\r\n".encode("latin-1")]
                    for name, value in event.get("headers", []):
                        out.append(bytes(name) + b": " + bytes(value) + b"\r\n")
                    # Each connection serves a single request, like the WSGI worker.
                    out.append(b"Connection: close\r\n\r\n")
                    writer.write(b"".join(out))
                    response_started = True
                elif event["type"] == "http.response.body":
                    writer.write(event.get("body", b""))
                    await writer.drain()

            try:
                await self.app(scope, receive, send)
            except Exception:
                if not response_started:
                    writer.write(
                        b"HTTP/1.1 500 Internal Server Error\r\n"
                        b"Content-Length: 0\r\n"
                        b"Connection: close\r\n\r\n"
                    )
                raise
        finally:
            try:
                await writer.drain()
            except (ConnectionError, OSError):
                pass
            writer.close()
            try:
                await writer.wait_closed()
            except (ConnectionError, OSError):
                pass
