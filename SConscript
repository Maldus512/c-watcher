import os

def rchop(s, suffix):
    if suffix and s.endswith(suffix):
        return s[:-len(suffix)]
    return s

Import("c_watcher_env")

sources = Glob("./src/*.c")

try:
    Import('c_watcher_suffix')
    objects = [c_watcher_env.Object(
        f"{rchop(x.get_abspath(), '.c')}-{c_watcher_suffix}", x) for x in sources]
except:
    objects = [c_watcher_env.Object(x) for x in sources]

path = Dir('.').srcnode().abspath
result = (objects, list(map(lambda x: os.path.join(path, x), ["src"])))
Return("result")
