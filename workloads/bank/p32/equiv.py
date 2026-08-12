import target_before
import target_after


def make_subclass(base):
    class Msg(base):
        __slots__ = ("token", "user", "conn_type", "ip_address", "port",
                     "privileged", "unknown", "obfuscated_port", "status")
        __excluded_attrs__ = {"unknown"}

    return Msg


def build(base, specs):
    Msg = make_subclass(base)
    out = []
    for spec in specs:
        m = Msg()
        for k, v in spec.items():
            setattr(m, k, v)
        out.append(m)
    return out


def fill(spec):
    base = {"token": 0, "user": "", "conn_type": "P", "ip_address": "0.0.0.0",
            "port": 0, "privileged": False, "unknown": 99,
            "obfuscated_port": 0, "status": 0}
    base.update(spec)
    return base


# empty / single / typical / larger
cases = [
    [],
    [fill({"token": 1, "user": "alice"})],
    [fill({"token": i, "user": "u%d" % i, "port": 2234 + i}) for i in range(5)],
    [fill({"token": i, "user": "name_%d" % i, "status": i % 3}) for i in range(200)],
]

equiv = True
for specs in cases:
    before_msgs = build(target_before.InternalMessage, specs)
    after_msgs = build(target_after.InternalMessage, specs)
    before_out = [str(m) for m in before_msgs]
    after_out = [str(m) for m in after_msgs]
    if before_out != after_out:
        equiv = False
        break

print("EQUIV=%s" % ("yes" if equiv else "no"))
