import sys
import zipimport


def _precache_zipimporters(path=None):
    pic = sys.path_importer_cache
    path = path or sys.path
    for entry_path in path:
        if entry_path not in pic:
            try:
                pic[entry_path] = zipimport.zipimporter(entry_path)
            except zipimport.ZipImportError:
                continue
    return pic
