import os

sources = {}

Import("c_watcher_env")

objects = [c_watcher_env.Object(x) for x in Glob("./src/*.c")]

result = (objects, list(map(lambda x: os.path.join(os.getcwd(), x), ["src"])))
Return("result")
