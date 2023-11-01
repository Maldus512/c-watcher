import os

Import("c_watcher_env")

objects = [c_watcher_env.Object(x) for x in Glob("./src/*.c")]

path = Dir('.').srcnode().abspath
result = (objects, list(map(lambda x: os.path.join(path, x), ["src"])))
Return("result")
