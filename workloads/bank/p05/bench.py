import os
import shutil
import tempfile
import time

import target


def build_reader_and_html(workdir):
	# Source directory holds the dictionary file and its referenced resources.
	src_dir = os.path.join(workdir, 'src')
	os.makedirs(src_dir, exist_ok=True)
	dict_filename = os.path.join(src_dir, 'mydict.mdx')
	# touch the dict file (only its dirname is used)
	open(dict_filename, 'wb').close()

	# Create several referenced resource files (.css / .js) with realistic-ish sizes.
	ref_names = []
	for i in range(6):
		for ext in ('.css', '.js'):
			name = 'res_%d%s' % (i, ext)
			path = os.path.join(src_dir, name)
			with open(path, 'wb') as f:
				f.write(b'/* resource data */ ' + (b'x' * 8192))
			ref_names.append(name)

	resources_dir = os.path.join(workdir, 'cache', 'mydict')
	href_root_dir = '/api/cache/mydict/'

	reader = target.MDictReader(dict_filename, resources_dir, href_root_dir)

	# Build a definition HTML referencing each resource by filename in quotes.
	parts = []
	for name in ref_names:
		if name.endswith('.css'):
			parts.append('<link rel="stylesheet" href="%s">' % name)
		else:
			parts.append('<script src="%s"></script>' % name)
	html = '<html><head>' + ''.join(parts) + '</head><body>definition</body></html>'
	return reader, resources_dir, html


def run_once(reader, resources_dir, html):
	# Each "lookup" reprocesses the same HTML for both .css and .js extensions.
	reader._fix_file_path(html, '.css')
	reader._fix_file_path(html, '.js')


def main():
	workdir = tempfile.mkdtemp(prefix='mdict_bench_')
	try:
		reader, resources_dir, html = build_reader_and_html(workdir)
		ITERS = 4000

		# warmup
		run_once(reader, resources_dir, html)

		t0 = time.perf_counter()
		for _ in range(ITERS):
			run_once(reader, resources_dir, html)
		t1 = time.perf_counter()
		print('TIME_SECONDS=%f' % (t1 - t0))
	finally:
		shutil.rmtree(workdir, ignore_errors=True)


if __name__ == '__main__':
	main()
