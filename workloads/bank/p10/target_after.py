'''Extracted kernel: Relation.selection (after optimization).

The optimization compiles the selection expression once into a code
object (compile(..., 'eval')) instead of letting eval() re-parse the
expression string on every tuple of the relation.
'''


def is_valid_relation_name(name):
    '''A relation/attribute name is valid if it is a valid python identifier.'''
    return name.isidentifier()


class rstring(str):
    '''A string that can autocast itself to int or float when possible.'''

    def autocast(self):
        '''Returns the value, casted to int or float if possible.'''
        try:
            return int(self)
        except ValueError:
            pass
        try:
            return float(self)
        except ValueError:
            pass
        return str(self)


class Header(tuple):
    '''Header of a relation: a tuple of unique, valid attribute names.'''

    def __new__(cls, fields):
        return super(Header, cls).__new__(cls, tuple(fields))

    def __init__(self, *args, **kwargs):
        for i in self:
            if not is_valid_relation_name(i):
                raise Exception('"%s" is not a valid attribute name' % i)
        if len(self) != len(set(self)):
            raise Exception('Attribute names must be unique')


class Relation(object):
    def __init__(self):
        self.header = Header([])
        self.content = set()

    def selection(self, expr):
        '''
        Selection, expr must be a valid Python expression; can contain field names.
        '''
        newt = relation()
        newt.header = Header(self.header)

        c_expr = compile(expr, 'selection', 'eval')

        for i in self.content:
            # Fills the attributes dictionary with the values of the tuple
            attributes = {attr: i[j].autocast()
                          for j, attr in enumerate(self.header)
                          }

            try:
                if eval(c_expr, attributes):
                    newt.content.add(i)
            except Exception as e:
                raise Exception(
                    "Failed to evaluate %s\n%s" % (expr, e.__str__()))
        return newt


relation = Relation
