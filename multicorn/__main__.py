import multicorn


if __name__ == "__main__":
    server = multicorn.Server("multicorn", "WSGIWorker", 4)
    server.run(("localhost", 3000))
