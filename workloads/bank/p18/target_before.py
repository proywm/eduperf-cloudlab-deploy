from threading import Lock

# Map: plugin_name -> plugin_instance
plugins = {}

# Map: plugin_name -> init_lock to make sure that a plugin isn't initialized
# multiple times
plugins_init_locks = {}


def get_plugin(plugin_name, reload=False):
    """ Registers a plugin instance by name if not registered already, or
        returns the registered plugin instance"""
    global plugins
    global plugins_init_locks

    if plugin_name not in plugins_init_locks:
        plugins_init_locks[plugin_name] = Lock()

    with plugins_init_locks[plugin_name]:
        if plugin_name in plugins and not reload:
            return plugins[plugin_name]

    raise RuntimeError('plugin not registered: {}'.format(plugin_name))
