import multicorn


if __name__ == "__main__":
    server = multicorn.Server("multicorn.workers", "WSGIWorker", 4)
    server.run(("localhost", 3000))
