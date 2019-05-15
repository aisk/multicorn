import asyncio
import asyncio.events
import asyncio.futures
import signal
import time

from _event_ffi import ffi, lib
from .dd import dd


_callback = None
_callback_args = None


@ffi.def_extern()
def signal_handler(sig, events, arg):
    lib.event_base_loopbreak(arg)


@ffi.def_extern()
def timer_handler(_, events, args):
    dd['callback'](*dd['args'])
    try:
        dd['co'].send(None)
    except StopIteration:
        pass


class EventLoop(asyncio.events.AbstractEventLoop):
    def __init__(self):
        self.base = lib.event_base_new()
        event = lib.event_new(self.base, signal.SIGINT.value, lib.EV_SIGNAL | lib.EV_PERSIST, lib.signal_handler, self.base)
        lib.event_add(event, ffi.NULL)

    def get_debug(self):
        return True

    def create_future(self):
        return asyncio.futures.Future()

    def run_forever(self):
        lib.event_base_dispatch(self.base)

    def call_later(self, delay, callback, *args, context=None):
        dd['callback'] = callback
        dd['args'] = args

        event = lib.event_new(self.base, -1, lib.EV_TIMEOUT, lib.timer_handler, ffi.NULL)
        timeval = lib.new_timeval(1, delay)
        lib.event_add(event, timeval)

        def cancel():
            print('TimerHandler cancel callback called!')
        return asyncio.TimerHandle(time.time() + delay, cancel, None, self)

    def _timer_handle_cancelled(self, handle):
        # TODO: finish this shit.
        return False


class EventLoopPolicy(asyncio.events.BaseDefaultEventLoopPolicy):
    def _loop_factory(self):
        return EventLoop()
