import socket
import threading
import time
from typing import Any, Iterable

from backports import interpreters


class Server:
    def __init__(self, worker: str, worker_args: Iterable[Any] = (), *, workers_count: int = 4):
        splited = worker.split(":")
        self.module_name = splited[0]
        self.class_name = splited[1]

        self.worker_args = worker_args

        self.workers_count = workers_count

    def _run_worker(self, bind, fd):
        interpreter = interpreters.create()
        source = f"""
from {self.module_name} import {self.class_name}
# worker = {self.class_name}(*{[repr(x) for x in self.worker_args]})
worker = {self.class_name}(*{self.worker_args})
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
            time.sleep(0.1)
        [t.join() for t in threads]
