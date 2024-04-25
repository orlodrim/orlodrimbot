#!/usr/bin/python3

import argparse
import glob
import os
import shutil
import subprocess
import tempfile

import create_templates_stats


def run_test(dump_dir, compact_format=False, update_expectations=False):
    # Initialize dump_dir.
    # Since it's somewhat inconvenient to maintain a test input in xml format, the test
    # input is stored in a simpler text format and converted to xml at the beginning of
    # the test.
    simple_dump_path = os.path.join(
        create_templates_stats.BOT_CODE_DIR,
        "templates_stats/testdata/integration_test/input_dump.txt",
    )
    xml_dump_path = os.path.join(
        dump_dir, "frwiki-20000101-pages-meta-current1.xml.bz2"
    )
    convert_dump_command = create_templates_stats.Command(
        "%BOT_BIN%/dump/processing/testtools/create_xml_dump"
    )
    bzip_command = create_templates_stats.Command("bzip2")
    with open(simple_dump_path, "rb") as simple_dump_file:
        with open(xml_dump_path, "wb") as xml_dump_file:
            create_templates_stats.pipe_commands(
                [convert_dump_command, bzip_command],
                stdin=simple_dump_file,
                stdout=xml_dump_file,
            )
    with open(os.path.join(dump_dir, "luadb_wiki.txt"), "w") as f:
        f.write("<!-- Empty for the test -->")

    # Function being tested.
    create_templates_stats.create_templates_stats(dump_dir, "2000-01-01", has_templates_dump=False, compact_format=compact_format)

    # Check the output.
    expected_actual_pairs = (
        ("luadb_auto.txt", "luadb_auto.txt"),
        ("stats_main_namespaces.txt", "stat-templates/data/json.dat"),
        ("stats_all_namespaces.txt", "stat-templates/data-allns/json.dat"),
    )
    for expected_file, actual_file in expected_actual_pairs:
        full_expected_file = os.path.join(
            create_templates_stats.BOT_CODE_DIR,
            "templates_stats/testdata/integration_test",
            expected_file,
        )
        full_actual_file = os.path.join(dump_dir, actual_file)
        if update_expectations:
            shutil.copy(full_actual_file, full_expected_file)
        else:
            subprocess.run(
                ["diff", "-u", full_expected_file, full_actual_file],
                check=True,
            )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--testdir",
        help=(
            "If set, run the test in this directory instead of creating a temporary "
            "directory. This is convenient if you need to inspect the output manually."
        ),
    )
    parser.add_argument(
        "--autoupdate",
        action="store_true",
        help="Update expectations based on the test output",
    )
    args = parser.parse_args()

    if args.testdir:
        test_dir = args.testdir
        os.makedirs(test_dir, exist_ok=True)
    else:
        temp_dir = tempfile.TemporaryDirectory()
        test_dir = temp_dir.name
    run_test(test_dir, update_expectations=args.autoupdate)


if __name__ == "__main__":
    main()
