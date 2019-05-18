import asyncio

import sonia
from sonia.event_loop import EventLoopPolicy


print(sonia.get_event_version())

sonia.start_http_server()

# asyncio.set_event_loop_policy(EventLoopPolicy())
#
# async def test():
#     print('test called!')
#     await asyncio.sleep(1)
#     print('test ended!')
#
# asyncio.run(test())
