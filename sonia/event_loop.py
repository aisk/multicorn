import asyncio.futures
import asyncio.events

from _event_ffi import lib


class EventLoop(asyncio.events.AbstractEventLoop):
    def __init__(self):
        self.base = lib.event_base_new()

    def get_debug(self):
        return False

    def create_future(self):
        return asyncio.futures.Future()

    def run_forever(self):
        lib.event_base_dispatch(self.base)


class EventLoopPolicy(asyncio.events.BaseDefaultEventLoopPolicy):
    def _loop_factory(self):
        return EventLoop()
