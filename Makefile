CXX=g++
CXXFLAGS=-std=c++17 -Wall -Werror -O2 -I.

MWCLIENT_OBJECT_FILES= \
	cbl/args_parser.o \
	cbl/date.o \
	cbl/error.o \
	cbl/file.o \
	cbl/html_entities.o \
	cbl/http_client.o \
	cbl/json.o \
	cbl/log.o \
	cbl/path.o \
	cbl/string.o \
	cbl/unicode_fr.o \
	cbl/utf8.o \
	mwclient/bot_exclusion.o \
	mwclient/parser.o \
	mwclient/parser_misc.o \
	mwclient/parser_nodes.o \
	mwclient/request.o \
	mwclient/site_info.o \
	mwclient/titles_util.o \
	mwclient/util/init_wiki.o \
	mwclient/wiki_base.o \
	mwclient/wiki.o \
	mwclient/wiki_defs.o \
	mwclient/wiki_read_api.o \
	mwclient/wiki_read_api_query_list.o \
	mwclient/wiki_read_api_query_prop.o \
	mwclient/wiki_session.o \
	mwclient/wiki_write_api.o

MWCLIENT_TESTS_DEPENDENCIES=\
	cbl/unittest.o \
	mwclient/mock_wiki.o \
	mwclient/tests/parser_test_util.o \
	mwclient/libmwclient.a

MWCLIENT_TESTS= \
	cbl/path_test \
	mwclient/tests/parser_misc_test \
	mwclient/tests/parser_nodes_test \
	mwclient/tests/parser_test

WIKIUTIL_OBJECT_FILES=\
	orlodrimbot/wikiutil/date_parser.o

ORLODRIMBOT_TESTS_DEPENDENCIES=\
	cbl/unittest.o \
	mwclient/mock_wiki.o \
	mwclient/libmwclient.a \
	orlodrimbot/wikiutil/wikiutil.a

ORLODRIMBOT_TESTS=\
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_test \
	orlodrimbot/wikiutil/date_parser_test

all: mwclient/libmwclient.a orlodrimbot/sandbox/sandbox orlodrimbot/bot_requests_archiver/bot_requests_archiver
check: $(MWCLIENT_TESTS) $(ORLODRIMBOT_TESTS)
	for bin in $^; do $${bin}; done; echo SUCCESS
clean:
	rm -f cbl/*.o cbl/*_test
	rm -f mwclient/*.o mwclient/*/*.o mwclient/*/*_test mwclient/libmwclient.a
	rm -f orlodrimbot/*/*.o orlodrimbot/*/*_test
	rm -f orlodrimbot/bot_requests_archiver/bot_requests_archiver
	rm -f orlodrimbot/sandbox/sandbox
	rm -f orlodrimbot/wikiutil/wikiutil.a

# cbl + mwclient

mwclient/libmwclient.a: $(MWCLIENT_OBJECT_FILES)
	ar rcs $@ $^
cbl/%_test: cbl/%_test.o $(MWCLIENT_TESTS_DEPENDENCIES)
	$(CXX) -o $@ $^ -lcurl
mwclient/%_test: mwclient/%_test.o $(MWCLIENT_TESTS_DEPENDENCIES)
	$(CXX) -o $@ $^ -lcurl

# OrlodrimBot

orlodrimbot/wikiutil/%_test: orlodrimbot/wikiutil/%_test.o $(ORLODRIMBOT_TESTS_DEPENDENCIES)
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/wikiutil/wikiutil.a: $(WIKIUTIL_OBJECT_FILES)
	ar rcs $@ $^

orlodrimbot/sandbox/sandbox: orlodrimbot/sandbox/sandbox.o orlodrimbot/sandbox/sandbox_lib.o mwclient/libmwclient.a
	$(CXX) -o $@ $^ -lcurl

orlodrimbot/bot_requests_archiver/bot_requests_archiver: \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver.o \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.o \
	mwclient/libmwclient.a orlodrimbot/wikiutil/wikiutil.a
	$(CXX) -o $@ $^ -lcurl -lre2
orlodrimbot/bot_requests_archiver/bot_requests_archiver_test: \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib_test.o \
	orlodrimbot/bot_requests_archiver/bot_requests_archiver_lib.o \
	$(ORLODRIMBOT_TESTS_DEPENDENCIES)
	$(CXX) -o $@ $^ -lcurl -lre2

.PHONY: all check clean
