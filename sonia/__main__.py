import sonia


if __name__ == "__main__":
    server = sonia.Server("sonia", "WSGIWorker", 4)
    print(server)
    server.run(("localhost", 3000))
