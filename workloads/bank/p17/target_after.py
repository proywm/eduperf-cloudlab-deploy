def declaration_path( decl ):
    """
    returns a list of parent declarations names

    @return: [names], where first item contains top parent name and last item
             contains decl name
    """
    if not decl:
        return []
    if not decl.cache.declaration_path:
        result = [ decl.name ]
        parent = decl.parent
        while parent:
            if parent.cache.declaration_path:
                result.reverse()
                decl.cache.declaration_path = parent.cache.declaration_path + result
                return decl.cache.declaration_path
            else:
                result.append( parent.name )
                parent = parent.parent
        result.reverse()
        decl.cache.declaration_path = result
        return result
    else:
        return decl.cache.declaration_path
