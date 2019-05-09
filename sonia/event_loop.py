import asyncio.futures
import asyncio.events
import signal

from _event_ffi import ffi, lib


@ffi.def_extern()
def signal_handler(sig, events, arg):
    lib.event_base_loopbreak(arg)


class EventLoop(asyncio.events.AbstractEventLoop):
    def __init__(self):
        self.base = lib.event_base_new()
        event = lib.event_new(self.base, signal.SIGINT.value, lib.EV_SIGNAL | lib.EV_PERSIST, lib.signal_handler, self.base)
        lib.event_add(event, ffi.NULL)

    def get_debug(self):
        return False

    def create_future(self):
        return asyncio.futures.Future()

    def run_forever(self):
        lib.event_base_dispatch(self.base)


class EventLoopPolicy(asyncio.events.BaseDefaultEventLoopPolicy):
    def _loop_factory(self):
        return EventLoop()
