#!/usr/bin/python3

import itertools
import os
import re

_RE_INCLUDE = re.compile('#include ([<"])([^"<>]+)')

_LIB_BY_HEADER = {
  'curl/curl.h': 'curl',
  're2/re2.h': 're2',
  'sqlite3.h': 'sqlite3',
}


def dfs(root, get_children):
  todo = [root]
  visited = {id(root)}
  while todo:
    item = todo.pop()
    yield item
    for child in get_children(item):
      if id(child) not in visited:
        visited.add(id(child))
        todo.append(child)


class File:

  def __init__(self, path):
    self.path = path
    self.implemented_header_path = None
    self.path_without_ext, ext = os.path.splitext(self.path)
    self.is_source = ext == '.cpp'
    self.is_test = self.is_source and self.path_without_ext.endswith('_test')
    self.has_main_function = False
    self.headers_paths = []
    self.headers = []
    self.sources = []
    self.library = None
    self.external_libs = []
    self._load_content()

  def _get_path_from_root(self, path):
    return path if '/' in path else os.path.join(os.path.dirname(self.path), path)

  def _load_content(self):
    with open(self.path) as f:
      for line in f:
        include_match = _RE_INCLUDE.match(line)
        if include_match:
          header = include_match.group(2)
          if include_match.group(1) == '"':
            self.headers_paths.append(self._get_path_from_root(header))
          elif header in _LIB_BY_HEADER:
            self.external_libs.append(_LIB_BY_HEADER[header])
        elif self.is_source and line.startswith('int main('):
          self.has_main_function = True
        elif line.startswith('// IMPLEMENTS:'):
          self.implemented_header_path = self._get_path_from_root(line[len('// IMPLEMENTS:'):].strip())

  def resolve_direct_dependencies(self, all_files):
    self.headers = [all_files[path] for path in self.headers_paths]
    if self.is_source:
      header = all_files.get(self.implemented_header_path or self.path_without_ext + '.h')
      if header:
        header.sources.append(self)

  def get_code_dependencies(self):
    deps = [header.path for header in dfs(self, lambda file: file.headers)]
    return [deps[0]] + sorted(deps[1:])

  def get_bin_dependencies(self):
    objects = []
    libraries = set()
    external_libs = set()
    for file in dfs(self, lambda file: itertools.chain(file.headers, file.sources)):
      if file.library:
        libraries.add((file.library.sort_key, file.library.path))
      elif file.is_source:
        objects.append(file.path_without_ext + '.o')
      external_libs.update(file.external_libs)
    return ([objects[0]] + sorted(objects[1:]) + [path for _, path in sorted(libraries)], sorted(external_libs))

  def add_to_library(self, library):
    if self.has_main_function:
      raise RuntimeError(f'File with main function added to library: {self.path}')
    self.library = library
    if self.is_source:
      library.objects.add(self.path_without_ext + '.o')

  def add_to_library_rec(self, library):
    def add_rec(file):
      file.add_to_library(library)
      for child in itertools.chain(file.headers, file.sources):
        if not child.library:
          add_rec(child)
    add_rec(self)


class Library:

  def __init__(self, path, sort_key):
    self.path = path
    self.sort_key = sort_key
    self.objects = set()


def enum_targets():
  for (dir_path, dir_names, file_names) in os.walk('.'):
    if dir_path == '.':
      dir_names.remove('.git')
    for file_name in file_names:
      _, extension = os.path.splitext(file_name)
      if extension in ['.h', '.cpp']:
        yield os.path.join(dir_path[2:], file_name)


def format_rule(target, dependencies, command, max_line_length=120):
  content = target + ':'
  length = len(content)
  for dependency in dependencies:
    length += len(dependency) + 3
    if length > max_line_length:
      content += ' \\\n\t' + dependency
      length = 8 + len(dependency)
    else:
      content += ' ' + dependency
  content += f'\n\t{command}\n'
  return content


def replace_section(content, start_marker, end_marker, section_content):
  start = content.find(start_marker)
  if start == -1:
    raise RuntimeError(f'"{start_marker}" not found')
  start += len(start_marker)
  end = content.find(end_marker)
  if end == -1:
    raise RuntimeError(f'"{end_marker}" not found')
  return content[:start] + section_content + content[end:]


def main():
  all_files = {}
  for path in enum_targets():
    all_files[path] = File(path)
  for file in all_files.values():
    file.resolve_direct_dependencies(all_files)

  mwclient_lib = Library('mwclient/libmwclient.a', 2)
  wikiutil_lib = Library('orlodrimbot/wikiutil/libwikiutil.a', 1)
  for file in all_files.values():
    if file.path.startswith('mwclient/') and not file.is_test and not file.path.startswith('mwclient/tests/'):
      file.add_to_library_rec(mwclient_lib)
    elif file.path.startswith('orlodrimbot/wikiutil/') and not file.is_test:
      file.add_to_library(wikiutil_lib)

  rules = []
  tests = []
  binaries = []
  for path, file in sorted(all_files.items()):
    if not file.is_source:
      continue
    rules.append(format_rule(file.path_without_ext + '.o', file.get_code_dependencies(),
                             '$(CXX) $(CXXFLAGS) -c -o $@ $<'))
    if file.has_main_function:
      objects, external_libs = file.get_bin_dependencies()
      external_libs_command = ''.join(' -l' + lib for lib in external_libs)
      rules.append(format_rule(file.path_without_ext, objects, '$(CXX) -o $@ $^' + external_libs_command))
      if file.is_test:
        tests.append(file.path_without_ext)
      else:
        binaries.append(file.path_without_ext)
  for library in [mwclient_lib, wikiutil_lib]:
    rules.append(format_rule(library.path, sorted(library.objects), 'ar rcs $@ $^'))

  with open('Makefile', 'r') as f:
    content = f.read()
  content = replace_section(content, '# autogenerated-lists-begin\n', '# autogenerated-lists-end\n',
                            'BINARIES= \\\n\t{binaries}\nTESTS= \\\n\t{tests}\n'.format(
                                binaries=' \\\n\t'.join(binaries), tests=' \\\n\t'.join(tests)))
  content = replace_section(content, '# autogenerated-rules-begin\n', '# autogenerated-rules-end\n', ''.join(rules))
  with open('Makefile', 'w') as f:
    f.write(content)


if __name__ == '__main__':
  main()
