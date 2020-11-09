import socket
from wsgiref import simple_server


class EchoWorker:
    def run(self, bind, fd):
        sock = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
        while True:
            client, address = sock.accept()
            with client:
                while True:
                    data = client.recv(1024)
                    if not data:
                        break
                    client.sendall(data)


class WSGIServer(simple_server.WSGIServer):
    def server_bind(self):
        host, port = self.server_address[:2]
        self.server_name = socket.getfqdn(host)
        self.server_port = port
        self.setup_environ()


class WSGIWorker:
    def run(self, bind, fd):
        server = WSGIServer(bind, simple_server.WSGIRequestHandler)
        server.socket = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
        server.set_app(simple_server.demo_app)
        server.serve_forever()
