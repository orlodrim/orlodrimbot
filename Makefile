CC=g++ -std=c++17 -Wall -Werror -O2 -I.

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

MWCLIENT_TESTLIB_OBJECT_FILES=\
	cbl/unittest.o \
	mwclient/tests/parser_test_util.o

MWCLIENT_TESTS= \
	cbl/path_test \
	mwclient/tests/parser_misc_test \
	mwclient/tests/parser_nodes_test \
	mwclient/tests/parser_test

all: mwclient/libmwclient.a orlodrimbot/sandbox/sandbox
check: $(MWCLIENT_TESTS)
	for bin in $^; do $${bin}; done; echo SUCCESS
clean:
	rm -f cbl/*.o mwclient/*.o mwclient/util/*.o mwclient/libmwclient.a
	rm -f orlodrimbot/sandbox/*.o orlodrimbot/sandbox/sandbox

%.o: %.cpp
	$(CC) -c $< -o $@
mwclient/libmwclient.a: $(MWCLIENT_OBJECT_FILES)
	ar rcs $@ $^
%_test: %_test.o $(MWCLIENT_TESTLIB_OBJECT_FILES) mwclient/libmwclient.a
	g++ -o $@ $^ -lcurl
orlodrimbot/sandbox/sandbox: orlodrimbot/sandbox/sandbox.o orlodrimbot/sandbox/sandbox_lib.o mwclient/libmwclient.a
	g++ -o $@ $^ -lcurl

.PHONY: all check clean orlodrimbot-obj
