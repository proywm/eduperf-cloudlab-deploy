import sys
import zipimport


def _precache_zipimporters(path=None):
    pic = sys.path_importer_cache
    path = set(path or sys.path)
    path.difference_update(pic)
    for entry_path in path:
        try:
            pic[entry_path] = zipimport.zipimporter(entry_path)
        except zipimport.ZipImportError:
            continue
    return pic
