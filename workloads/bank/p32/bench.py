import time
import target


def make_subclass():
    # Real subclass with many slots, mirroring messages like PeerInit/Login
    class Msg(target.InternalMessage):
        __slots__ = ("token", "user", "conn_type", "ip_address", "port",
                     "privileged", "unknown", "obfuscated_port", "status",
                     "avgspeed", "uploadnum", "files", "dirs", "country")
        __excluded_attrs__ = {"unknown"}

    return Msg


def build_instances(n):
    Msg = make_subclass()
    instances = []
    for i in range(n):
        m = Msg()
        m.token = i
        m.user = "user_%d" % i
        m.conn_type = "P"
        m.ip_address = "192.168.0.%d" % (i % 256)
        m.port = 2234 + (i % 1000)
        m.privileged = bool(i % 2)
        m.unknown = 0
        m.obfuscated_port = 0
        m.status = i % 3
        m.avgspeed = i * 10
        m.uploadnum = i
        m.files = i * 2
        m.dirs = i
        m.country = "US"
        instances.append(m)
    return instances


def main():
    instances = build_instances(2000)
    iterations = 200

    start = time.perf_counter()
    total = 0
    for _ in range(iterations):
        for m in instances:
            total += len(str(m))
    elapsed = time.perf_counter() - start

    print("CHECKSUM=%d" % total)
    print("TIME_SECONDS=%.6f" % elapsed)


if __name__ == "__main__":
    main()
