import importlib
import socket
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
            raise ValueError(f"app must be 'module' and 'attr' joined by a colon, got {app!r}")

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
