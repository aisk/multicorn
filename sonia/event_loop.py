import asyncio
import asyncio.events
import asyncio.futures
import signal
import time

from _event_ffi import ffi, lib
from .dd import dd


@ffi.def_extern()
def signal_handler(sig, events, arg):
    lib.event_base_loopbreak(arg)


@ffi.def_extern()
def timer_handler(_, events, args):
    handle = ffi.from_handle(args)
    handle._run()
    try:
        dd['co'].send(None)
    except StopIteration:
        pass


@ffi.def_extern()
def event_callback(base, event, args):
    # print('event_callback called:', base, event, args)
    return 0


class EventLoop(asyncio.events.AbstractEventLoop):
    def __init__(self):
        self._debug = False
        self.base = lib.event_base_new()
        event = lib.event_new(self.base, signal.SIGINT.value, lib.EV_SIGNAL | lib.EV_PERSIST, lib.signal_handler, self.base)
        lib.event_add(event, ffi.NULL)

    def set_debug(self, debug):
        self._debug = debug

    def get_debug(self):
        return self._debug

    def create_future(self):
        return asyncio.futures.Future()

    def run_forever(self):
        while True:
            lib.event_base_foreach_event(self.base, lib.event_callback, ffi.NULL)
            lib.event_base_loop(self.base, lib.EVLOOP_ONCE)

    def call_later(self, delay, callback, *args, context=None):
        handle = asyncio.TimerHandle(time.time() + delay, callback, args, self)
        self.userdata = ffi.new_handle(handle)
        event = lib.event_new(self.base, -1, lib.EV_TIMEOUT, lib.timer_handler, self.userdata)
        timeval = lib.new_timeval(1, delay)
        lib.event_add(event, timeval)
        return handle

    def _timer_handle_cancelled(self, handle):
        # TODO: finish this shit.
        return False

    def run_until_complete(self, future):
        try:
            future.send(None)
        except StopIteration:
            pass

    async def shutdown_asyncgens(self):
        # TODO: finish this
        ...

    def close(self):
        lib.event_base_free(self.base)

class EventLoopPolicy(asyncio.events.BaseDefaultEventLoopPolicy):
    def _loop_factory(self):
        return EventLoop()
