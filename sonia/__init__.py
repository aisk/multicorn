import socket
import threading
import time

from backports import interpreters


class Worker:
    pass


class EchoWorker:
    def run(self, bind):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        sock.bind(bind)
        sock.listen()
        while True:
            client, address = sock.accept()
            print(client, address)
            with client:
                while True:
                    data = client.recv(1024)
                    if not data:
                        break
                    client.sendall(data)
                    time.sleep(0.0001)  # For now (Python3.8), we need this for current interpreter to yield.


class Server:
    def __init__(self, module_name: str, class_name: str, /,
                 workers_count: int = 4):
        self.module_name = module_name
        self.class_name = class_name
        self.workers_count = workers_count

    def _run_worker(self, bind):
        interpreter = interpreters.create()
        source = f"""
from {self.module_name} import {self.class_name}
worker = {self.class_name}()
worker.run({bind})
"""
        interpreter.run(source)

    def run(self, bind):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        sock.bind(bind)
        sock.listen()
        threads = []
        for _ in range(self.workers_count):
            t = threading.Thread(target=self._run_worker, args=[bind])
            t.start()
            threads.append(t)
        [t.join() for t in threads]
