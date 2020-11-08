import sonia


if __name__ == "__main__":
    server = sonia.Server("sonia", "EchoWorker")
    print(server)
    server.run(("localhost", 3000))
