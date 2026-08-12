import os
import shutil
from pathlib import Path


class MDictReader:
	"""
	Minimal extraction of MDictReader._fix_file_path (before optimization).
	Only the attributes used by the function are kept.
	"""
	def __init__(self, filename: 'str', resources_dir: 'str', href_root_dir: 'str') -> 'None':
		self.filename = filename
		self._resources_dir = resources_dir
		self._href_root_dir = href_root_dir

	def _fix_file_path(self, definition_html: 'str', file_extension: 'str') -> 'str':
		extension_position = 0
		while (extension_position := definition_html.find(file_extension, extension_position)) != -1:
			filename_position = definition_html.rfind('"', 0, extension_position) + 1
			filename = definition_html[filename_position:extension_position + len(file_extension)]
			file_path_on_disk =  os.path.join(os.path.dirname(self.filename), filename)
			if os.path.isfile(file_path_on_disk):
				# Create the resource directory
				Path(self._resources_dir).mkdir(parents=True, exist_ok=True)
				# Copy the file to the resource directory
				shutil.copy(file_path_on_disk, os.path.join(self._resources_dir, filename))
				definition_html = definition_html[:filename_position] + self._href_root_dir + definition_html[filename_position:]
			extension_position += len(file_extension)
		return definition_html
