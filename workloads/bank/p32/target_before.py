class InternalMessage:
    __slots__ = ()
    __excluded_attrs__ = {}
    msg_type = "INTERNAL"

    def __str__(self):
        attrs = {s: self.__getattribute__(s) for s in self.__slots__ if s not in self.__excluded_attrs__}
        return f"<{self.msg_type} - {self.__class__.__name__}> {attrs}"
