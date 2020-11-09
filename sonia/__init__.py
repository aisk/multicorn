import socket
import threading

from backports import interpreters


class Worker:
    pass


class EchoWorker:
    def run(self, bind, fd):
        sock = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
        while True:
            client, address = sock.accept()
            print(client, address)
            with client:
                while True:
                    data = client.recv(1024)
                    if not data:
                        break
                    client.sendall(data)


class Server:
    def __init__(self, module_name: str, class_name: str, /,
                 workers_count: int = 4):
        self.module_name = module_name
        self.class_name = class_name
        self.workers_count = workers_count

    def _run_worker(self, bind, fd):
        interpreter = interpreters.create()
        source = f"""
from {self.module_name} import {self.class_name}
worker = {self.class_name}()
worker.run({bind}, {fd})
"""
        interpreter.run(source)

    def run(self, bind):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(bind)
        sock.listen()
        threads = []
        for _ in range(self.workers_count):
            t = threading.Thread(target=self._run_worker, args=[bind, sock.fileno()])
            t.start()
            threads.append(t)
        [t.join() for t in threads]
