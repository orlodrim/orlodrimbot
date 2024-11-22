#!/usr/bin/python3
"""Compute template statistics for wstat.fr from an XML dump of the French Wikipedia."""

import argparse
import glob
import os
import subprocess
import tempfile


BOT_CODE_DIR = os.path.join(os.environ["WIKIPEDIA_CODE_ROOT"], "orlodrimbot")
BOT_BIN_DIR = os.path.join(os.environ["WIKIPEDIA_BIN_ROOT"], "orlodrimbot")


class Command:

    def __init__(self, path, args=None, **kwargs):
        self.command = [
            path.replace("%BOT_CODE%", BOT_CODE_DIR).replace("%BOT_BIN%", BOT_BIN_DIR)
        ] + (args or [])
        self.kwargs = kwargs
        self.process = None

    def start(self, verbose=True, **kwargs):
        args = dict(self.kwargs)
        args.update(kwargs)
        if verbose:
            print(f"+ {self}")
        self.process = subprocess.Popen(self.command, **args)

    def wait(self):
        self.process.wait()
        if self.process.returncode != 0:
            raise RuntimeError(
                f"{self.process.args} failed with code {self.process.returncode}"
            )

    def run(self):
        self.start()
        self.wait()

    def __str__(self):
        return " ".join(self.command)


def pipe_commands(commands, stdin=None, stdout=None):
    command_stdin = stdin
    print("+ " + " \\\n    | ".join(str(command) for command in commands))
    for i, command in enumerate(commands):
        command.start(
            verbose=False,
            stdin=command_stdin,
            stdout=subprocess.PIPE if i < len(commands) - 1 else stdout,
        )
        command_stdin = command.process.stdout
    for command in commands:
        command.wait()


def get_bzcat_dump_command(dump_dir):
    dump_files_pattern = os.path.join(dump_dir, "frwiki-*-pages-meta-current*.xml*.bz2")
    dump_files = glob.glob(dump_files_pattern)
    if not dump_files:
        raise RuntimeError(f"No files matching {dump_files_pattern}")
    return Command("bzcat", dump_files)


def sort_with_c_order(input_files, output_file):
    if not isinstance(input_files, list):
        input_files = [input_files]
    Command("sort", input_files + ["-o", output_file], env={"LC_ALL": "C"}).run()


def create_dump_of_templates(dump_dir):
    """Runs the minimum subset of the global dump processing required to extract template statistics."""
    with open(os.path.join(dump_dir, "dummy-disambig-re2.txt"), "w") as f:
        f.write(r"dummy-regexp-for-disambiguation-pages-detection")
    bzcat_command = get_bzcat_dump_command(dump_dir)
    process_command = Command(
        "%BOT_BIN%/dump/processing/processing",
        [
            f"--datadir={dump_dir}",
            "--processes=modules,templates,titles",
            "--modules-params=output:modules.dat",
            "--templates-params=output:templates.dat",
            "--titles-params=input_disambigregexp:dummy-disambig-re2.txt,output:titles-unsorted.dat",
        ],
    )
    pipe_commands([bzcat_command, process_command])
    sort_with_c_order(
        os.path.join(dump_dir, "titles-unsorted.dat"),
        os.path.join(dump_dir, "titles.dat"),
    )
    Command("%BOT_CODE%/dump/processing/list_redirects.sh", [dump_dir]).run()


def extract_templates_inclusions(dump_dir, stats_dir, compact_format):
    with open(os.path.join(dump_dir, "templates-redirections.dat"), "w") as f:
        Command(
            "grep", ["^Modèle:", os.path.join(dump_dir, "redirections.dat")], stdout=f
        ).run()

    Command(
        "%BOT_BIN%/templates_stats/parse_templates",
        [
            f"--templatesdump={dump_dir}/templates.dat",
            f"--withparam={stats_dir}/templates-for-stats.dat",
            f"--withparamnames={stats_dir}/templates-for-stats-names.dat",
            f"--templatedata={dump_dir}/luadb_auto.txt",
        ],
    ).run()
    with open(os.path.join(stats_dir, "modules-for-stats-names.dat"), "wb") as f:
        Command(
            "grep", ["^[^ ]", os.path.join(dump_dir, "modules.dat")], stdout=f
        ).run()
    with open(os.path.join(stats_dir, "modules-for-stats.dat"), "wb") as f:
        Command(
            "sed",
            [r"s/.*/\0|/", os.path.join(stats_dir, "modules-for-stats-names.dat")],
            stdout=f,
        ).run()
    sort_with_c_order(
        [
            os.path.join(stats_dir, "templates-for-stats-names.dat"),
            os.path.join(stats_dir, "modules-for-stats-names.dat"),
        ],
        os.path.join(stats_dir, "templates-and-modules-for-stats-names.dat"),
    )
    sort_with_c_order(
        [
            os.path.join(stats_dir, "templates-for-stats.dat"),
            os.path.join(stats_dir, "modules-for-stats.dat"),
        ],
        os.path.join(stats_dir, "templates-and-modules-for-stats.dat"),
    )
    with open(os.path.join(dump_dir, "luadb_merged.txt"), "wb") as f:
        Command(
            "cat",
            [
                os.path.join(dump_dir, "luadb_auto.txt"),
                os.path.join(dump_dir, "luadb_wiki.txt"),
            ],
            stdout=f,
        ).run()

    bzcat_command = get_bzcat_dump_command(dump_dir)
    extract_command = Command(
        "%BOT_BIN%/templates_stats/extract_templates",
        [
            f"--redirects={dump_dir}/templates-redirections.dat",
            f"--templates-names={stats_dir}/templates-and-modules-for-stats-names.dat",
        ],
    )
    sort_command = Command(
        "sort", ["--compress-program=/bin/gzip"], env={"LC_ALL": "C"}
    )
    if compact_format:
        index_command = Command(
            "%BOT_BIN%/templates_stats/create_indexed_extraction",
            [f"--index={stats_dir}/extraction-index.dat"],
        )
    else:
        index_command = Command('cat')
    extraction_path = os.path.join(stats_dir, "extraction-sorted.dat")
    with open(extraction_path, "wb") as index_file:
        pipe_commands(
            [bzcat_command, extract_command, sort_command, index_command],
            stdout=index_file,
        )
    return extraction_path


def compute_stats_from_extraction(
    dump_date,
    lua_db_path,
    templates_dump_path,
    extraction_path,
    output_dir,
    all_namespaces,
):
    os.makedirs(output_dir, exist_ok=True)
    stats_args = [
        f"--luadb={lua_db_path}",
        f"--inclusions={extraction_path}",
        f"--templates={templates_dump_path}",
        f"--jsonoutput={output_dir}",
        f"--list-by-count={output_dir}/templates-direct-inclusions.dat",
        f"--dumpdate={dump_date}",
        "--format=json",
    ]
    if not all_namespaces:
        stats_args += ["--nouser", "--notalk"]
    stats_command = Command("%BOT_BIN%/templates_stats/stat", stats_args)
    stats_command.run()


def create_templates_stats(dump_dir, dump_date, has_templates_dump, compact_format):
    lua_db_path = os.path.join(dump_dir, "luadb_merged.txt")
    stats_dir = os.path.join(dump_dir, "stat-templates")
    templates_dump_path = os.path.join(stats_dir, "templates-and-modules-for-stats.dat")

    if not has_templates_dump:
        create_dump_of_templates(dump_dir)

    os.makedirs(stats_dir, exist_ok=True)
    extraction_path = extract_templates_inclusions(dump_dir, stats_dir, compact_format=compact_format)

    for subdir, all_namespaces in (("data", False), ("data-allns", True)):
        compute_stats_from_extraction(
            dump_date,
            lua_db_path,
            templates_dump_path,
            extraction_path,
            os.path.join(stats_dir, subdir),
            all_namespaces=all_namespaces,
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dumpdir",
        required=True,
        help=(
            "Directory of the downloaded Wikipedia dump. Should contain the "
            "pages-meta-current files of the dump "
            "(frwiki-*-pages-meta-current*.xml*.bz2) and luadb_wiki.txt "
            "(a copy of https://fr.wikipedia.org/wiki/Utilisateur:Orlodrim/LuaConfig)."
        ),
    )
    parser.add_argument(
        "--dumpdate", required=True, help="Date of the dump (YYYY-MM-DD)."
    )
    parser.add_argument(
        "--hastemplatesdump",
        action="store_true",
        help=(
            "If set, assume that the global dump processing has already been done and "
            "that all required output files are also in --dumpdir."
        ),
    )
    parser.add_argument(
        "--compactformat",
        action="store_true",
        help=(
            "If set, use a more compact format for the list of template inclusions (extraction-sorted.dat)"
        ),
    )
    args = parser.parse_args()
    create_templates_stats(
        args.dumpdir, args.dumpdate, has_templates_dump=args.hastemplatesdump, compact_format=args.compactformat,
    )


if __name__ == "__main__":
    main()
