import multicorn


if __name__ == "__main__":
    server = multicorn.Server("multicorn.workers:WSGIWorker", ["wsgiref.simple_server:demo_app"], workers_count=4)
    server.run(("localhost", 3000))
