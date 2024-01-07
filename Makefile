CXX=g++
CXXFLAGS=-std=c++2a -Wall -Werror -O2 -I.

# autogenerated-lists-begin
BINARIES= \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver \
	orlodrimbot/draft_moved_to_main/draft_moved_to_main \
	orlodrimbot/live_replication/live_replication \
	orlodrimbot/monthly_categories_init/monthly_categories_init \
	orlodrimbot/move_subpages/move_subpages \
	orlodrimbot/sandbox/sandbox \
	orlodrimbot/status_on_user_pages/check_status \
	orlodrimbot/talk_page_archiver/talk_page_archiver \
	orlodrimbot/update_main_page/update_main_page
TESTS= \
	cbl/path_test \
	mwclient/tests/parser_misc_test \
	mwclient/tests/parser_nodes_test \
	mwclient/tests/parser_test \
	mwclient/tests/wiki_log_events_test \
	mwclient/util/bot_section_test \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib_test \
	orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib_test \
	orlodrimbot/live_replication/recent_changes_reader_test \
	orlodrimbot/live_replication/recent_changes_sync_test \
	orlodrimbot/status_on_user_pages/check_status_lib_test \
	orlodrimbot/talk_page_archiver/archiver_test \
	orlodrimbot/talk_page_archiver/frwiki_algorithms_test \
	orlodrimbot/talk_page_archiver/thread_test \
	orlodrimbot/talk_page_archiver/thread_util_test \
	orlodrimbot/update_main_page/copy_page_test \
	orlodrimbot/wikiutil/date_formatter_test \
	orlodrimbot/wikiutil/date_parser_test
# autogenerated-lists-end

.PHONY: all check clean

all: $(BINARIES)
check: $(TESTS)
	for bin in $^; do if ! (cd $$(dirname $${bin}) && ./$$(basename $${bin})); then exit 1; fi; done
clean:
	rm -f */*.[ao] */*/*.[ao] $(BINARIES) $(TESTS)

# autogenerated-rules-begin
cbl/args_parser.o: cbl/args_parser.cpp cbl/args_parser.h cbl/error.h cbl/generated_range.h cbl/string.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/date.o: cbl/date.cpp cbl/date.h cbl/error.h cbl/log.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/error.o: cbl/error.cpp cbl/error.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/file.o: cbl/file.cpp cbl/error.h cbl/file.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/html_entities.o: cbl/html_entities.cpp cbl/html_entities.h cbl/utf8.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/http_client.o: cbl/http_client.cpp cbl/error.h cbl/generated_range.h cbl/http_client.h cbl/string.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/json.o: cbl/json.cpp cbl/error.h cbl/json.h cbl/utf8.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/log.o: cbl/log.cpp cbl/log.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/path.o: cbl/path.cpp cbl/path.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/path_test.o: cbl/path_test.cpp cbl/log.h cbl/path.h cbl/unittest.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/path_test: cbl/path_test.o cbl/unittest.o mwclient/libmwclient.a
	$(CXX) -o $@ $^
cbl/sqlite.o: cbl/sqlite.cpp cbl/error.h cbl/file.h cbl/log.h cbl/sqlite.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/string.o: cbl/string.cpp cbl/error.h cbl/generated_range.h cbl/string.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/tempfile.o: cbl/tempfile.cpp cbl/error.h cbl/tempfile.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/unicode_fr.o: cbl/unicode_fr.cpp cbl/unicode_fr.h cbl/utf8.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/unittest.o: cbl/unittest.cpp cbl/log.h cbl/unittest.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
cbl/utf8.o: cbl/utf8.cpp cbl/utf8.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/bot_exclusion.o: mwclient/bot_exclusion.cpp cbl/generated_range.h cbl/string.h mwclient/bot_exclusion.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/mock_wiki.o: mwclient/mock_wiki.cpp cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h \
	cbl/log.h cbl/string.h mwclient/mock_wiki.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/parser.o: mwclient/parser.cpp cbl/error.h cbl/generated_range.h cbl/log.h mwclient/parser.h \
	mwclient/parser_misc.h mwclient/parser_nodes.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/parser_misc.o: mwclient/parser_misc.cpp cbl/generated_range.h cbl/string.h mwclient/parser_misc.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/parser_nodes.o: mwclient/parser_nodes.cpp cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h \
	mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/request.o: mwclient/request.cpp cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h cbl/log.h \
	cbl/string.h mwclient/request.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/site_info.o: mwclient/site_info.cpp cbl/error.h cbl/json.h cbl/unicode_fr.h cbl/utf8.h \
	mwclient/site_info.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/parser_misc_test.o: mwclient/tests/parser_misc_test.cpp cbl/log.h cbl/unittest.h \
	mwclient/parser_misc.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/parser_misc_test: mwclient/tests/parser_misc_test.o cbl/unittest.o mwclient/libmwclient.a
	$(CXX) -o $@ $^
mwclient/tests/parser_nodes_test.o: mwclient/tests/parser_nodes_test.cpp cbl/error.h cbl/generated_range.h \
	cbl/log.h cbl/unittest.h mwclient/parser.h mwclient/parser_misc.h mwclient/parser_nodes.h \
	mwclient/tests/parser_test_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/parser_nodes_test: mwclient/tests/parser_nodes_test.o cbl/unittest.o \
	mwclient/tests/parser_test_util.o mwclient/libmwclient.a
	$(CXX) -o $@ $^
mwclient/tests/parser_test.o: mwclient/tests/parser_test.cpp cbl/error.h cbl/generated_range.h cbl/log.h \
	cbl/unittest.h mwclient/parser.h mwclient/parser_misc.h mwclient/parser_nodes.h \
	mwclient/tests/parser_test_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/parser_test: mwclient/tests/parser_test.o cbl/unittest.o mwclient/tests/parser_test_util.o \
	mwclient/libmwclient.a
	$(CXX) -o $@ $^
mwclient/tests/parser_test_util.o: mwclient/tests/parser_test_util.cpp cbl/error.h cbl/generated_range.h \
	mwclient/parser.h mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/tests/parser_test_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/replay_wiki.o: mwclient/tests/replay_wiki.cpp cbl/args_parser.h cbl/date.h cbl/error.h \
	cbl/file.h cbl/generated_range.h cbl/http_client.h cbl/json.h cbl/log.h cbl/string.h \
	mwclient/site_info.h mwclient/tests/replay_wiki.h mwclient/titles_util.h mwclient/util/init_wiki.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/wiki_log_events_test.o: mwclient/tests/wiki_log_events_test.cpp cbl/date.h cbl/error.h cbl/json.h \
	cbl/log.h cbl/unittest.h mwclient/site_info.h mwclient/tests/replay_wiki.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/tests/wiki_log_events_test: mwclient/tests/wiki_log_events_test.o cbl/unittest.o \
	mwclient/tests/replay_wiki.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
mwclient/titles_util.o: mwclient/titles_util.cpp cbl/generated_range.h cbl/html_entities.h cbl/json.h \
	cbl/string.h cbl/unicode_fr.h cbl/utf8.h mwclient/site_info.h mwclient/titles_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/util/bot_section.o: mwclient/util/bot_section.cpp cbl/date.h cbl/error.h cbl/generated_range.h \
	cbl/json.h cbl/string.h cbl/unicode_fr.h cbl/utf8.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/util/bot_section.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/util/bot_section_test.o: mwclient/util/bot_section_test.cpp cbl/date.h cbl/error.h cbl/json.h \
	cbl/log.h cbl/unittest.h mwclient/mock_wiki.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/util/bot_section.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/util/bot_section_test: mwclient/util/bot_section_test.o cbl/unittest.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl
mwclient/util/include_tags.o: mwclient/util/include_tags.cpp cbl/error.h cbl/generated_range.h cbl/string.h \
	mwclient/util/include_tags.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/util/init_wiki.o: mwclient/util/init_wiki.cpp cbl/args_parser.h cbl/date.h cbl/error.h cbl/file.h \
	cbl/generated_range.h cbl/json.h cbl/log.h cbl/path.h cbl/string.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/util/templates_by_name.o: mwclient/util/templates_by_name.cpp cbl/date.h cbl/error.h \
	cbl/generated_range.h cbl/json.h mwclient/parser.h mwclient/parser_misc.h mwclient/parser_nodes.h \
	mwclient/site_info.h mwclient/titles_util.h mwclient/util/templates_by_name.h mwclient/wiki.h \
	mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki.o: mwclient/wiki.cpp cbl/date.h cbl/error.h cbl/http_client.h cbl/json.h cbl/unicode_fr.h \
	cbl/utf8.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_base.o: mwclient/wiki_base.cpp cbl/date.h cbl/error.h cbl/http_client.h cbl/json.h cbl/log.h \
	mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_defs.o: mwclient/wiki_defs.cpp cbl/date.h cbl/error.h cbl/generated_range.h cbl/log.h \
	cbl/string.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_read_api.o: mwclient/wiki_read_api.cpp cbl/date.h cbl/error.h cbl/http_client.h cbl/json.h \
	mwclient/request.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_read_api_query_list.o: mwclient/wiki_read_api_query_list.cpp cbl/date.h cbl/error.h \
	cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/request.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_read_api_query_prop.o: mwclient/wiki_read_api_query_prop.cpp cbl/date.h cbl/error.h \
	cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/bot_exclusion.h mwclient/request.h \
	mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_session.o: mwclient/wiki_session.cpp cbl/date.h cbl/error.h cbl/file.h cbl/generated_range.h \
	cbl/http_client.h cbl/json.h cbl/log.h cbl/string.h mwclient/request.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
mwclient/wiki_write_api.o: mwclient/wiki_write_api.cpp cbl/date.h cbl/error.h cbl/json.h cbl/log.h \
	mwclient/request.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/bot_requests_archiver/bot_requests_archiver.o: orlodrimbot/bot_requests_archiver/bot_requests_archiver.cpp \
	cbl/args_parser.h cbl/date.h cbl/error.h cbl/json.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/bot_requests_archiver/bot_requests_archiver: orlodrimbot/bot_requests_archiver/bot_requests_archiver.o \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.o orlodrimbot/wikiutil/libwikiutil.a \
	mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.o: \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.cpp cbl/date.h cbl/error.h \
	cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/parser.h mwclient/parser_misc.h \
	mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib_test.o: \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib_test.cpp cbl/date.h cbl/error.h cbl/json.h \
	cbl/log.h cbl/unittest.h mwclient/mock_wiki.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib_test: \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib_test.o cbl/unittest.o \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.o orlodrimbot/wikiutil/libwikiutil.a \
	mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/draft_moved_to_main/draft_moved_to_main.o: orlodrimbot/draft_moved_to_main/draft_moved_to_main.cpp \
	cbl/args_parser.h cbl/date.h cbl/error.h cbl/json.h cbl/sqlite.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.h \
	orlodrimbot/live_replication/recent_changes_reader.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/draft_moved_to_main/draft_moved_to_main: orlodrimbot/draft_moved_to_main/draft_moved_to_main.o \
	cbl/sqlite.o orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.o \
	orlodrimbot/live_replication/continue_token.o orlodrimbot/live_replication/recent_changes_reader.o \
	orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2 -lsqlite3
orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.o: orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.cpp \
	cbl/date.h cbl/error.h cbl/file.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/sqlite.h \
	cbl/string.h mwclient/parser.h mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/util/bot_section.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.h \
	orlodrimbot/live_replication/recent_changes_reader.h orlodrimbot/wikiutil/date_formatter.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib_test.o: \
	orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib_test.cpp cbl/date.h cbl/error.h cbl/file.h \
	cbl/json.h cbl/log.h cbl/sqlite.h cbl/tempfile.h cbl/unittest.h mwclient/mock_wiki.h \
	mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.h \
	orlodrimbot/live_replication/mock_recent_changes_reader.h orlodrimbot/live_replication/recent_changes_reader.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib_test: \
	orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib_test.o cbl/sqlite.o cbl/tempfile.o cbl/unittest.o \
	orlodrimbot/draft_moved_to_main/draft_moved_to_main_lib.o orlodrimbot/live_replication/continue_token.o \
	orlodrimbot/live_replication/mock_recent_changes_reader.o orlodrimbot/live_replication/recent_changes_reader.o \
	orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2 -lsqlite3
orlodrimbot/live_replication/continue_token.o: orlodrimbot/live_replication/continue_token.cpp cbl/error.h \
	cbl/generated_range.h cbl/string.h orlodrimbot/live_replication/continue_token.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/live_replication.o: orlodrimbot/live_replication/live_replication.cpp cbl/args_parser.h \
	cbl/date.h cbl/error.h cbl/json.h cbl/path.h cbl/sqlite.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/live_replication/recent_changes_reader.h \
	orlodrimbot/live_replication/recent_changes_sync.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/live_replication: orlodrimbot/live_replication/live_replication.o cbl/sqlite.o \
	orlodrimbot/live_replication/continue_token.o orlodrimbot/live_replication/recent_changes_reader.o \
	orlodrimbot/live_replication/recent_changes_sync.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lsqlite3
orlodrimbot/live_replication/mock_recent_changes_reader.o: orlodrimbot/live_replication/mock_recent_changes_reader.cpp \
	cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/sqlite.h cbl/string.h \
	mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/live_replication/mock_recent_changes_reader.h orlodrimbot/live_replication/recent_changes_reader.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/recent_changes_reader.o: orlodrimbot/live_replication/recent_changes_reader.cpp \
	cbl/date.h cbl/error.h cbl/json.h cbl/log.h cbl/sqlite.h mwclient/wiki_defs.h \
	orlodrimbot/live_replication/continue_token.h orlodrimbot/live_replication/recent_changes_reader.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/recent_changes_reader_test.o: orlodrimbot/live_replication/recent_changes_reader_test.cpp \
	cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/sqlite.h cbl/string.h \
	cbl/tempfile.h cbl/unittest.h mwclient/mock_wiki.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/live_replication/recent_changes_reader.h orlodrimbot/live_replication/recent_changes_sync.h \
	orlodrimbot/live_replication/recent_changes_test_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/recent_changes_reader_test: orlodrimbot/live_replication/recent_changes_reader_test.o \
	cbl/sqlite.o cbl/tempfile.o cbl/unittest.o orlodrimbot/live_replication/continue_token.o \
	orlodrimbot/live_replication/recent_changes_reader.o orlodrimbot/live_replication/recent_changes_sync.o \
	orlodrimbot/live_replication/recent_changes_test_util.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lsqlite3
orlodrimbot/live_replication/recent_changes_sync.o: orlodrimbot/live_replication/recent_changes_sync.cpp cbl/date.h \
	cbl/error.h cbl/json.h cbl/log.h cbl/sqlite.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/live_replication/recent_changes_sync.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/recent_changes_sync_test.o: orlodrimbot/live_replication/recent_changes_sync_test.cpp \
	cbl/date.h cbl/error.h cbl/json.h cbl/log.h cbl/sqlite.h cbl/tempfile.h cbl/unittest.h \
	mwclient/mock_wiki.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/live_replication/recent_changes_sync.h \
	orlodrimbot/live_replication/recent_changes_test_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/live_replication/recent_changes_sync_test: orlodrimbot/live_replication/recent_changes_sync_test.o \
	cbl/sqlite.o cbl/tempfile.o cbl/unittest.o orlodrimbot/live_replication/recent_changes_sync.o \
	orlodrimbot/live_replication/recent_changes_test_util.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lsqlite3
orlodrimbot/live_replication/recent_changes_test_util.o: orlodrimbot/live_replication/recent_changes_test_util.cpp \
	cbl/date.h cbl/error.h cbl/json.h cbl/log.h mwclient/mock_wiki.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/live_replication/recent_changes_test_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/monthly_categories_init/monthly_categories_init.o: \
	orlodrimbot/monthly_categories_init/monthly_categories_init.cpp cbl/args_parser.h cbl/date.h cbl/error.h \
	cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/monthly_categories_init/monthly_categories_init: \
	orlodrimbot/monthly_categories_init/monthly_categories_init.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl
orlodrimbot/move_subpages/move_subpages.o: orlodrimbot/move_subpages/move_subpages.cpp cbl/args_parser.h \
	cbl/date.h cbl/error.h cbl/file.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/path.h \
	cbl/string.h mwclient/site_info.h mwclient/titles_util.h mwclient/util/init_wiki.h mwclient/wiki.h \
	mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/move_subpages/move_subpages_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/move_subpages/move_subpages: orlodrimbot/move_subpages/move_subpages.o \
	orlodrimbot/move_subpages/move_subpages_lib.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/move_subpages/move_subpages_lib.o: orlodrimbot/move_subpages/move_subpages_lib.cpp cbl/date.h \
	cbl/error.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/move_subpages/move_subpages_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/sandbox/sandbox.o: orlodrimbot/sandbox/sandbox.cpp cbl/args_parser.h cbl/date.h cbl/error.h \
	cbl/json.h mwclient/site_info.h mwclient/titles_util.h mwclient/util/init_wiki.h mwclient/wiki.h \
	mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/sandbox/sandbox_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/sandbox/sandbox: orlodrimbot/sandbox/sandbox.o orlodrimbot/sandbox/sandbox_lib.o \
	mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl
orlodrimbot/sandbox/sandbox_lib.o: orlodrimbot/sandbox/sandbox_lib.cpp cbl/date.h cbl/error.h cbl/json.h \
	cbl/log.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/sandbox/sandbox_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/status_on_user_pages/check_status.o: orlodrimbot/status_on_user_pages/check_status.cpp cbl/args_parser.h \
	cbl/date.h cbl/error.h cbl/json.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/status_on_user_pages/check_status_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/status_on_user_pages/check_status: orlodrimbot/status_on_user_pages/check_status.o \
	orlodrimbot/status_on_user_pages/check_status_lib.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl
orlodrimbot/status_on_user_pages/check_status_lib.o: orlodrimbot/status_on_user_pages/check_status_lib.cpp \
	cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/util/bot_section.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/status_on_user_pages/check_status_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/status_on_user_pages/check_status_lib_test.o: orlodrimbot/status_on_user_pages/check_status_lib_test.cpp \
	cbl/date.h cbl/error.h cbl/json.h cbl/log.h cbl/unittest.h mwclient/mock_wiki.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/status_on_user_pages/check_status_lib.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/status_on_user_pages/check_status_lib_test: orlodrimbot/status_on_user_pages/check_status_lib_test.o \
	cbl/unittest.o orlodrimbot/status_on_user_pages/check_status_lib.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl
orlodrimbot/talk_page_archiver/algorithm.o: orlodrimbot/talk_page_archiver/algorithm.cpp cbl/date.h cbl/error.h \
	cbl/json.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/talk_page_archiver/algorithm.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/archive_template.o: orlodrimbot/talk_page_archiver/archive_template.cpp cbl/date.h \
	cbl/error.h cbl/generated_range.h cbl/json.h cbl/string.h mwclient/parser.h mwclient/parser_misc.h \
	mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h mwclient/util/templates_by_name.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/talk_page_archiver/algorithm.h \
	orlodrimbot/talk_page_archiver/archive_template.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/archiver.o: orlodrimbot/talk_page_archiver/archiver.cpp cbl/date.h cbl/error.h \
	cbl/file.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/path.h cbl/string.h mwclient/parser.h \
	mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/talk_page_archiver/algorithm.h \
	orlodrimbot/talk_page_archiver/archive_template.h orlodrimbot/talk_page_archiver/archiver.h \
	orlodrimbot/talk_page_archiver/frwiki_algorithms.h orlodrimbot/talk_page_archiver/thread.h \
	orlodrimbot/talk_page_archiver/thread_util.h orlodrimbot/wikiutil/date_formatter.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/archiver_test.o: orlodrimbot/talk_page_archiver/archiver_test.cpp cbl/date.h \
	cbl/error.h cbl/file.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h cbl/tempfile.h \
	cbl/unittest.h mwclient/mock_wiki.h mwclient/parser.h mwclient/parser_misc.h mwclient/parser_nodes.h \
	mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/talk_page_archiver/algorithm.h orlodrimbot/talk_page_archiver/archive_template.h \
	orlodrimbot/talk_page_archiver/archiver.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/archiver_test: orlodrimbot/talk_page_archiver/archiver_test.o cbl/tempfile.o \
	cbl/unittest.o orlodrimbot/talk_page_archiver/algorithm.o orlodrimbot/talk_page_archiver/archive_template.o \
	orlodrimbot/talk_page_archiver/archiver.o orlodrimbot/talk_page_archiver/frwiki_algorithms.o \
	orlodrimbot/talk_page_archiver/thread.o orlodrimbot/talk_page_archiver/thread_util.o \
	orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/talk_page_archiver/frwiki_algorithms.o: orlodrimbot/talk_page_archiver/frwiki_algorithms.cpp cbl/date.h \
	cbl/error.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/parser.h \
	mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/talk_page_archiver/algorithm.h \
	orlodrimbot/talk_page_archiver/frwiki_algorithms.h orlodrimbot/talk_page_archiver/thread_util.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/frwiki_algorithms_test.o: orlodrimbot/talk_page_archiver/frwiki_algorithms_test.cpp \
	cbl/date.h cbl/error.h cbl/json.h cbl/log.h cbl/unittest.h mwclient/mock_wiki.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/talk_page_archiver/algorithm.h orlodrimbot/talk_page_archiver/frwiki_algorithms.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/frwiki_algorithms_test: orlodrimbot/talk_page_archiver/frwiki_algorithms_test.o \
	cbl/unittest.o orlodrimbot/talk_page_archiver/algorithm.o orlodrimbot/talk_page_archiver/frwiki_algorithms.o \
	orlodrimbot/talk_page_archiver/thread_util.o orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/talk_page_archiver/talk_page_archiver.o: orlodrimbot/talk_page_archiver/talk_page_archiver.cpp \
	cbl/args_parser.h cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h mwclient/parser.h \
	mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/util/init_wiki.h mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/talk_page_archiver/algorithm.h orlodrimbot/talk_page_archiver/archive_template.h \
	orlodrimbot/talk_page_archiver/archiver.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/talk_page_archiver: orlodrimbot/talk_page_archiver/talk_page_archiver.o \
	orlodrimbot/talk_page_archiver/algorithm.o orlodrimbot/talk_page_archiver/archive_template.o \
	orlodrimbot/talk_page_archiver/archiver.o orlodrimbot/talk_page_archiver/frwiki_algorithms.o \
	orlodrimbot/talk_page_archiver/thread.o orlodrimbot/talk_page_archiver/thread_util.o \
	orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/talk_page_archiver/thread.o: orlodrimbot/talk_page_archiver/thread.cpp cbl/date.h cbl/error.h \
	cbl/generated_range.h cbl/json.h cbl/log.h cbl/string.h mwclient/parser.h mwclient/parser_misc.h \
	mwclient/parser_nodes.h mwclient/site_info.h mwclient/titles_util.h mwclient/wiki.h mwclient/wiki_base.h \
	mwclient/wiki_defs.h orlodrimbot/talk_page_archiver/algorithm.h \
	orlodrimbot/talk_page_archiver/archive_template.h orlodrimbot/talk_page_archiver/thread.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/thread_test.o: orlodrimbot/talk_page_archiver/thread_test.cpp cbl/date.h \
	cbl/error.h cbl/json.h cbl/log.h cbl/unittest.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/talk_page_archiver/algorithm.h \
	orlodrimbot/talk_page_archiver/thread.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/thread_test: orlodrimbot/talk_page_archiver/thread_test.o cbl/unittest.o \
	orlodrimbot/talk_page_archiver/algorithm.o orlodrimbot/talk_page_archiver/archive_template.o \
	orlodrimbot/talk_page_archiver/thread.o orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/talk_page_archiver/thread_util.o: orlodrimbot/talk_page_archiver/thread_util.cpp cbl/date.h \
	cbl/error.h cbl/generated_range.h cbl/string.h mwclient/parser.h mwclient/parser_misc.h \
	mwclient/parser_nodes.h orlodrimbot/talk_page_archiver/thread_util.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/thread_util_test.o: orlodrimbot/talk_page_archiver/thread_util_test.cpp cbl/date.h \
	cbl/log.h cbl/unittest.h orlodrimbot/talk_page_archiver/thread_util.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/talk_page_archiver/thread_util_test: orlodrimbot/talk_page_archiver/thread_util_test.o cbl/unittest.o \
	orlodrimbot/talk_page_archiver/thread_util.o orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lre2
orlodrimbot/update_main_page/copy_page.o: orlodrimbot/update_main_page/copy_page.cpp cbl/date.h cbl/error.h \
	cbl/file.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/sqlite.h cbl/string.h mwclient/parser.h \
	mwclient/parser_misc.h mwclient/parser_nodes.h mwclient/request.h mwclient/site_info.h \
	mwclient/titles_util.h mwclient/util/bot_section.h mwclient/util/include_tags.h mwclient/wiki.h \
	mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/live_replication/recent_changes_reader.h \
	orlodrimbot/update_main_page/copy_page.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/update_main_page/copy_page_test.o: orlodrimbot/update_main_page/copy_page_test.cpp cbl/date.h \
	cbl/error.h cbl/file.h cbl/generated_range.h cbl/json.h cbl/log.h cbl/sqlite.h cbl/string.h \
	cbl/tempfile.h cbl/unittest.h mwclient/mock_wiki.h mwclient/site_info.h mwclient/titles_util.h \
	mwclient/wiki.h mwclient/wiki_base.h mwclient/wiki_defs.h \
	orlodrimbot/live_replication/mock_recent_changes_reader.h orlodrimbot/live_replication/recent_changes_reader.h \
	orlodrimbot/update_main_page/copy_page.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/update_main_page/copy_page_test: orlodrimbot/update_main_page/copy_page_test.o cbl/sqlite.o \
	cbl/tempfile.o cbl/unittest.o orlodrimbot/live_replication/continue_token.o \
	orlodrimbot/live_replication/mock_recent_changes_reader.o orlodrimbot/live_replication/recent_changes_reader.o \
	orlodrimbot/update_main_page/copy_page.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2 -lsqlite3
orlodrimbot/update_main_page/update_main_page.o: orlodrimbot/update_main_page/update_main_page.cpp cbl/args_parser.h \
	cbl/date.h cbl/error.h cbl/generated_range.h cbl/json.h cbl/sqlite.h cbl/string.h \
	mwclient/site_info.h mwclient/titles_util.h mwclient/util/init_wiki.h mwclient/wiki.h \
	mwclient/wiki_base.h mwclient/wiki_defs.h orlodrimbot/live_replication/recent_changes_reader.h \
	orlodrimbot/update_main_page/copy_page.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/update_main_page/update_main_page: orlodrimbot/update_main_page/update_main_page.o cbl/sqlite.o \
	orlodrimbot/live_replication/continue_token.o orlodrimbot/live_replication/recent_changes_reader.o \
	orlodrimbot/update_main_page/copy_page.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl -lre2 -lsqlite3
orlodrimbot/wikiutil/date_formatter.o: orlodrimbot/wikiutil/date_formatter.cpp cbl/date.h \
	orlodrimbot/wikiutil/date_formatter.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/wikiutil/date_formatter_test.o: orlodrimbot/wikiutil/date_formatter_test.cpp cbl/date.h cbl/log.h \
	cbl/unittest.h orlodrimbot/wikiutil/date_formatter.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/wikiutil/date_formatter_test: orlodrimbot/wikiutil/date_formatter_test.o cbl/unittest.o \
	orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^
orlodrimbot/wikiutil/date_parser.o: orlodrimbot/wikiutil/date_parser.cpp cbl/date.h cbl/unicode_fr.h cbl/utf8.h \
	orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/wikiutil/date_parser_test.o: orlodrimbot/wikiutil/date_parser_test.cpp cbl/date.h cbl/log.h \
	cbl/unittest.h orlodrimbot/wikiutil/date_parser.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
orlodrimbot/wikiutil/date_parser_test: orlodrimbot/wikiutil/date_parser_test.o cbl/unittest.o \
	orlodrimbot/wikiutil/libwikiutil.a mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lre2
mwclient/libmwclient.a: cbl/args_parser.o cbl/date.o cbl/error.o cbl/file.o cbl/html_entities.o \
	cbl/http_client.o cbl/json.o cbl/log.o cbl/path.o cbl/string.o cbl/unicode_fr.o cbl/utf8.o \
	mwclient/bot_exclusion.o mwclient/mock_wiki.o mwclient/parser.o mwclient/parser_misc.o \
	mwclient/parser_nodes.o mwclient/request.o mwclient/site_info.o mwclient/titles_util.o \
	mwclient/util/bot_section.o mwclient/util/include_tags.o mwclient/util/init_wiki.o \
	mwclient/util/templates_by_name.o mwclient/wiki.o mwclient/wiki_base.o mwclient/wiki_defs.o \
	mwclient/wiki_read_api.o mwclient/wiki_read_api_query_list.o mwclient/wiki_read_api_query_prop.o \
	mwclient/wiki_session.o mwclient/wiki_write_api.o
	ar rcs $@ $^
orlodrimbot/wikiutil/libwikiutil.a: orlodrimbot/wikiutil/date_formatter.o orlodrimbot/wikiutil/date_parser.o
	ar rcs $@ $^
# autogenerated-rules-end
