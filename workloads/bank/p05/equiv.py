import os
import shutil
import tempfile

import target_before as B
import target_after as A


def build(workdir, ref_names):
	src_dir = os.path.join(workdir, 'src')
	os.makedirs(src_dir, exist_ok=True)
	dict_filename = os.path.join(src_dir, 'mydict.mdx')
	open(dict_filename, 'wb').close()
	for name in ref_names:
		with open(os.path.join(src_dir, name), 'wb') as f:
			f.write(b'data ' + name.encode())
	resources_dir = os.path.join(workdir, 'cache', 'mydict')
	href_root_dir = '/api/cache/mydict/'
	return dict_filename, resources_dir, href_root_dir


def make_html(ref_names):
	parts = []
	for name in ref_names:
		parts.append('<link href="%s">' % name)
	return '<html>' + ''.join(parts) + '</html>'


CASES = {
	'empty': ([], '', '.css'),
	'single': (['a.css'], None, '.css'),
	'typical': (['a.css', 'b.css', 'c.css'], None, '.css'),
	'larger': (['f%d.css' % i for i in range(20)], None, '.css'),
	'no_match': (['a.css'], '<html>no refs here</html>', '.css'),
}


def run_version(mod, dict_filename, resources_dir, href_root_dir, html, ext, repeats):
	reader = mod.MDictReader(dict_filename, resources_dir, href_root_dir)
	out = None
	for _ in range(repeats):
		out = reader._fix_file_path(html, ext)
	# capture resulting cache dir contents (mutation on disk)
	if os.path.isdir(resources_dir):
		files = sorted(os.listdir(resources_dir))
	else:
		files = []
	return out, files


def main():
	all_equiv = True
	for case_name, (ref_names, html_override, ext) in CASES.items():
		html = html_override if html_override is not None else make_html(ref_names)
		# Run each version twice to exercise the "already copied" path.
		wb = tempfile.mkdtemp(prefix='eq_b_')
		wa = tempfile.mkdtemp(prefix='eq_a_')
		try:
			db, rb, hb = build(wb, ref_names)
			da, ra, ha = build(wa, ref_names)
			out_b, files_b = run_version(B, db, rb, hb, html, ext, repeats=2)
			out_a, files_a = run_version(A, da, ra, ha, html, ext, repeats=2)
			if out_b != out_a or files_b != files_a:
				all_equiv = False
				print('MISMATCH in %s' % case_name)
		finally:
			shutil.rmtree(wb, ignore_errors=True)
			shutil.rmtree(wa, ignore_errors=True)

	print('EQUIV=%s' % ('yes' if all_equiv else 'no'))


if __name__ == '__main__':
	main()
