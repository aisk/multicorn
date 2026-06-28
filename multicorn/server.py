import os
import signal
import socket
import sys
import threading
import traceback
from typing import Any, Iterable, List

from backports import interpreters


class Server:
    def __init__(self, worker: str, worker_args: Iterable[Any] = (), *, workers_count: int = 4):
        module_name, sep, class_name = worker.partition(":")
        if not sep or not module_name or not class_name:
            raise ValueError(f"worker must be 'module' and 'class' joined by a colon, got {worker!r}")
        self.module_name = module_name
        self.class_name = class_name

        self.worker_args = tuple(worker_args)

        self.workers_count = workers_count

        self._errors: List[BaseException] = []

    def _run_worker(self, bind, fd, family):
        interpreter = interpreters.create()
        args = ", ".join(repr(arg) for arg in self.worker_args)
        source = (
            f"from {self.module_name} import {self.class_name}\n"
            f"worker = {self.class_name}({args})\n"
            f"worker.run({bind!r}, {fd}, {int(family)})\n"
        )
        try:
            interpreter.exec(source)
        except BaseException as exc:  # worker crashed or failed to start
            self._errors.append(exc)
        finally:
            # A sub-interpreter must be destroyed from the thread that created
            # it, so close it here instead of from the main thread.
            try:
                interpreter.close()
            except Exception:
                pass

    def run(self, bind):
        info = socket.getaddrinfo(bind[0], bind[1], type=socket.SOCK_STREAM)[0]
        family, socktype, proto, _, sockaddr = info
        sock = socket.socket(family, socktype, proto)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(sockaddr)
        sock.listen()

        stop = threading.Event()

        def handler(signum, frame):
            stop.set()

        try:
            previous = {
                sig: signal.signal(sig, handler)
                for sig in (signal.SIGINT, signal.SIGTERM)
            }
        except ValueError:
            # signal handlers can only be installed from the main thread
            previous = {}

        threads = []
        for _ in range(self.workers_count):
            t = threading.Thread(
                target=self._run_worker,
                args=(sockaddr, sock.fileno(), family),
                daemon=True,
            )
            t.start()
            threads.append(t)

        try:
            while not stop.is_set() and not self._errors:
                if not any(t.is_alive() for t in threads):
                    break
                stop.wait(0.1)
        finally:
            for sig, prev in previous.items():
                signal.signal(sig, prev)
            sock.close()

        # On a startup failure the failed workers exit on their own and close
        # their interpreters from their own threads; wait for that to finish.
        if self._errors:
            for t in threads:
                t.join(timeout=2)

        for exc in self._errors:
            traceback.print_exception(type(exc), exc, exc.__traceback__)

        if any(t.is_alive() for t in threads):
            # The remaining workers are blocked inside serve_forever and can't
            # be joined; live sub-interpreters crash interpreter finalization,
            # so terminate the process directly with a meaningful exit code.
            sys.stderr.flush()
            os._exit(1 if self._errors else 0)

        if self._errors:
            raise RuntimeError(f"{len(self._errors)} worker(s) failed") from self._errors[0]
