#include "mwclient/parser.h"
#include <algorithm>
#include <string>
#include <string_view>
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "parser_test_util.h"

using std::string;
using std::string_view;

namespace wikicode {

// A simple recursive computation of node depth, used as a reference to check the depth computation in the parser.
static int getNodeDepthRecursive(const Node& node) {
  int innerDepth = 0;
  switch (node.type()) {
    case NT_LIST: {
      for (const Node& item : node.asList()) {
        innerDepth = std::max(innerDepth, getNodeDepthRecursive(item));
      }
      break;
    }
    case NT_TEXT:
    case NT_COMMENT:
      break;
    case NT_TAG: {
      const Tag& tag = node.asTag();
      if (tag.content()) {
        innerDepth = getNodeDepthRecursive(*tag.content());
      }
      break;
    }
    case NT_LINK: {
      const Link& link = node.asLink();
      for (int i = 0; i < link.size(); i++) {
        innerDepth = std::max(innerDepth, getNodeDepthRecursive(link[i]));
      }
      break;
    }
    case NT_TEMPLATE: {
      const Template& template_ = node.asTemplate();
      for (int i = 0; i < template_.size(); i++) {
        innerDepth = std::max(innerDepth, getNodeDepthRecursive(template_[i]));
      }
      break;
    }
    case NT_VARIABLE: {
      const Variable& variable = node.asVariable();
      innerDepth = getNodeDepthRecursive(variable.nameNode());
      if (variable.defaultValue()) {
        innerDepth = std::max(innerDepth, getNodeDepthRecursive(*variable.defaultValue()));
      }
      break;
    }
  }
  return innerDepth + 1;
}

class ParserTest : public cbl::Test {
private:
  static void checkParsing(const string& code, const string& expectedDebugString) {
    List parsedCode = parse(code);
    string debugString = getNodeDebugString(parsedCode);
    CBL_ASSERT_EQ(debugString, expectedDebugString) << code;
    CBL_ASSERT_EQ(parsedCode.toString(), code);
    CBL_ASSERT_EQ(getNodeDepthRecursive(parsedCode), parser_internal::getCodeDepth(code)) << code;
  }
  CBL_TEST_CASE(Parsing) {
    checkParsing("", "list()");
    checkParsing("a", "list(text(a))");
    checkParsing("ab", "list(text(ab))");
    checkParsing("{{test}}", "list(template(list(text(test))))");
    checkParsing("{a}", "list(text({a}))");
    checkParsing("{{a}}", "list(template(list(text(a))))");
    checkParsing("{{{a}}}", "list(var(list(text(a))))");
    checkParsing("{{{{a}}}}", "list(text({),var(list(text(a))),text(}))");
    checkParsing("{{{{{a}}}}}", "list(template(list(var(list(text(a))))))");
    checkParsing("{{{{{a}} }}}", "list(var(list(template(list(text(a))),text( ))))");
    checkParsing("{{{{{{a}}}}}}", "list(var(list(var(list(text(a))))))");
    checkParsing("{{a{{{b}}{{c}}}d}}",
                 "list(template(list(text(a{),template(list(text(b))),template(list(text(c))),text(}d))))");
    checkParsing("{{a{{a}}}}", "list(template(list(text(a),template(list(text(a))))))");
    checkParsing("{{a{{{b}}<nowiki>{{c}}}d}}",
                 "list(template(list(text(a{),template(list(text(b))),text(<nowiki>),"
                 "template(list(text(c))),text(}d))))");
    checkParsing("{{a|b=c}}", "list(template(list(text(a)),list(text(b=c))))");
    checkParsing("{{a|b=c|d}}", "list(template(list(text(a)),list(text(b=c)),list(text(d))))");
    checkParsing("{{{a|b}}}", "list(var(list(text(a)),list(text(b))))");
    checkParsing("{{{a|b|c}}}", "list(var(list(text(a)),list(text(b|c))))");
    checkParsing("<!-- a -->", "list(comment(<!-- a -->))");
    checkParsing("<!----> a -->", "list(comment(<!---->),text( a -->))");
    checkParsing("<!---> a -->", "list(comment(<!---> a -->))");
    checkParsing("<!--> a -->", "list(comment(<!--> a -->))");
    checkParsing("<!--", "list(comment(<!--))");
    checkParsing("<!-", "list(text(<!-))");
    checkParsing("[[Target]]", "list(link(list(text(Target))))");
    checkParsing("[[Target]]<!-- A -->", "list(link(list(text(Target))),comment(<!-- A -->))");
    checkParsing("[[Target|Text]]", "list(link(list(text(Target)),list(text(Text))))");
    checkParsing("[[File:A.png|A|B|C]]",
                 "list(link(list(text(File:A.png)),list(text(A)),list(text(B)),list(text(C))))");
    // Ideally, a link should have at most two fields if the target is not a file. However, in general, deciding
    // if the target is a file requires expanding all templates, so it is not doable at the level of the parser.
    checkParsing("[[Target|A|B]]", "list(link(list(text(Target)),list(text(A)),list(text(B))))");
    // MediaWiki keeps the first ']' of ']]]' inside the link if there is a matching '['. This is not supported yet.
    // checkParsing("[[a|[b]]]", "list(link(list(text(a)),list(text([b]))))");

    checkParsing("<math>{{x}}</math>", "list(tag(<math>,list(text({{x}})),</math>))");
    checkParsing("1<math>{{x}}</math>2", "list(text(1),tag(<math>,list(text({{x}})),</math>),text(2))");
    checkParsing("<nowiki/>", "list(tag(<nowiki/>))");
    checkParsing("<nowiki<nowiki/>", "list(text(<nowiki),tag(<nowiki/>))");
    checkParsing("<nowiki />", "list(tag(<nowiki />))");
    checkParsing("<nowiki></nowiki>", "list(tag(<nowiki>,list(),</nowiki>))");
    checkParsing("<nowiki>a</nowiki><nowiki>b</nowiki><nowiki>c</nowiki>",
                 "list(tag(<nowiki>,list(text(a)),</nowiki>),tag(<nowiki>,list(text(b)),</nowiki>),"
                 "tag(<nowiki>,list(text(c)),</nowiki>))");
    checkParsing("<nowiki>{{x}}</nowiki>", "list(tag(<nowiki>,list(text({{x}})),</nowiki>))");
    checkParsing("<pre>{{x}}</pre>", "list(tag(<pre>,list(text({{x}})),</pre>))");
    checkParsing("<PRE>{{x}}</PRE>", "list(tag(<PRE>,list(text({{x}})),</PRE>))");
    checkParsing("<ref name=x>{{Ouvrage}}</ref>", "list(tag(<ref name=x>,list(template(list(text(Ouvrage)))),</ref>))");
    checkParsing("<score>{{x}}</score>", "list(tag(<score>,list(text({{x}})),</score>))");
    checkParsing("<source>{{x}}</source>", "list(tag(<source>,list(text({{x}})),</source>))");
    checkParsing("<templatedata>{\"key\":\"[[value]]\"}</templatedata>",
                 "list(tag(<templatedata>,list(text({\"key\":\"[[value]]\"})),</templatedata>))");
    checkParsing("<timeline>{{x}}</timeline>", "list(tag(<timeline>,list(text({{x}})),</timeline>))");
    // https://fr.wikipedia.org/w/index.php?oldid=93664625
    checkParsing("<source_a>a</source_a>", "list(text(<source_a>a</source_a>))");
    // https://fr.wikipedia.org/w/index.php?diff=148209735
    checkParsing("<poem>{{x}}</poem>", "list(tag(<poem>,list(template(list(text(x)))),</poem>))");
    checkParsing("<categorytree>X</categorytree>", "list(tag(<categorytree>,list(text(X)),</categorytree>))");
    checkParsing("<math>abc</nowiki>", "list(text(<math>abc</nowiki>))");
    checkParsing("<math><nowiki></math></nowiki>", "list(tag(<math>,list(text(<nowiki>)),</math>),text(</nowiki>))");
    checkParsing("<math><math></math>", "list(tag(<math>,list(text(<math>)),</math>))");
    checkParsing("<pre></pre>[[x]]", "list(tag(<pre>,list(),</pre>),link(list(text(x))))");
    checkParsing("<nowiki/>[[x]]", "list(tag(<nowiki/>),link(list(text(x))))");
    checkParsing("<nowiki><pre>a</pre></nowiki>", "list(tag(<nowiki>,list(text(<pre>a</pre>)),</nowiki>))");
    // Here, the parser does not reproduce MediaWiki behavior, which seems to parse <nowiki> tags within <pre> tags.
    // However, for applications other than rendering, it should not matter much.
    checkParsing("<pre><nowiki>a</nowiki></pre>", "list(tag(<pre>,list(text(<nowiki>a</nowiki>)),</pre>))");
    checkParsing("<nowiki>a", "list(text(<nowiki>a))");
    checkParsing("<pre>a", "list(tag(<pre>,list(text(a))))");
    checkParsing("<ref><!--</ref>a", "list(tag(<ref>,list(comment(<!--)),</ref>),text(a))");
    checkParsing("<references><ref></references><references><ref></ref></references>",
                 "list(tag(<references>,list(text(<ref>)),</references>),"
                 "tag(<references>,list(tag(<ref>,list(),</ref>)),</references>))");

    checkParsing("[[target|<poem>]]", "list(link(list(text(target)),list(text(<poem>))))");
    checkParsing("[[a|{{a]]", "list(link(list(text(a)),list(text({{a))))");
    checkParsing("[[a|{{a|]]", "list(link(list(text(a)),list(text({{a)),list()))");
    // Corner case where "{{" does not prevent "]]" from matching "[[", as long as there is no "|" after "{{".
    // Not supported for now.
    // checkParsing("[[a|{{a]]}}", "list(link(list(text(a)),list(text({{a))),text(}}))");
    checkParsing("[[a|{{a|]]}}", "list(text([[a|),template(list(text(a)),list(text(]]))))");
    checkParsing("{{a|[[}}", "list(text({{a|[[}}))");
    checkParsing("{{a|[[b|}}]]}}", "list(template(list(text(a)),list(link(list(text(b)),list(text(}}))))))");
    checkParsing("{{a|<poem>}}", "list(template(list(text(a)),list(text(<poem>))))");
    checkParsing("[[target|{{gras|<poem>]]", "list(link(list(text(target)),list(text({{gras)),list(text(<poem>))))");
    checkParsing("[[target|{{gras|<poem>]]}}", "list(text([[target|),template(list(text(gras)),list(text(<poem>]]))))");
    checkParsing("[[target|{{gras|<poem>}}]]",
                 "list(link(list(text(target)),list(template(list(text(gras)),list(text(<poem>))))))");

    checkParsing("[[[test]]", "list(text([[[test]]))");
    checkParsing("[[[[test]]", "list(text([[),link(list(text(test))))");
    checkParsing("[[[[[test]]", "list(text([[[[[test]]))");
    checkParsing("[[File:X|[[[test]]]]", "list(link(list(text(File:X)),list(text([[[test]]))))");
    checkParsing("{{a|[[[a}}", "list(text({{a|[[[a}}))");
    checkParsing("{{a|[[[a]]}}", "list(template(list(text(a)),list(text([[[a]]))))");
    checkParsing("{{a|[[[b|c]]}}", "list(template(list(text(a)),list(text([[[b|c]]))))");
  }

  CBL_TEST_CASE(MaxDepth) {
    int oldDepth = parser_internal::setParserMaxDepth(4);
    checkParsing("{{[[a]]}}", "list(text({{),link(list(text(a))),text(}}))");
    parser_internal::setParserMaxDepth(5);
    checkParsing("{{[[a]]}}", "list(template(list(link(list(text(a))))))");
    parser_internal::setParserMaxDepth(oldDepth);
  }

  CBL_TEST_CASE(MaxDepthLargeExample) {
    constexpr int totalDepth = 10000;
    constexpr int supportedDepth = 1000;
    string code;
    string debugString = "list(text(";
    for (int i = totalDepth; i >= 1; i--) {
      code += "[[{{";
      if (i > supportedDepth) {
        debugString += "[[{{";
      } else {
        if (i == supportedDepth) debugString += "),";
        debugString += "link(list(template(list(";
      }
    }
    code += "inside";
    debugString += "text(inside)";
    for (int i = 1; i <= totalDepth; i++) {
      code += "}}]]";
      if (i > supportedDepth) {
        debugString += "}}]]";
      } else {
        debugString += "))))";
        if (i == supportedDepth) debugString += ",text(";
      }
    }
    debugString += "))";

    int oldDepth = parser_internal::setParserMaxDepth(supportedDepth * 4 + 1);
    checkParsing(code, debugString);
    parser_internal::setParserMaxDepth(oldDepth);
  }

  CBL_TEST_CASE(ManyNestedTags) {
    string code;
    for (int i = 0; i < 50000; i++) {
      code += "<ref>";
    }
    checkParsing(code, "list(text(" + code + "))");
  }

  static void checkParseError(string_view code, const string& expectedError) {
    bool exceptionThrown = false;
    try {
      parse(code, STRICT);
    } catch (const ParseError& error) {
      CBL_ASSERT_EQ(error.what(), expectedError);
      exceptionThrown = true;
    }
    CBL_ASSERT(exceptionThrown) << code;
  }
  CBL_TEST_CASE(ParseError) {
    checkParseError("[[Link", "1:1:Unclosed link '[[Link'");
    checkParseError("Link]]", "1:5:Link closure without opening ']]'");
    checkParseError("[[Link\n]]", "1:1:Link whose target contains a line break '[[Link ]]'");
    checkParseError("{{Template", "1:1:Unclosed template '{{Template'");
    checkParseError("{{{Variable", "1:1:Unclosed variable '{{{Variable'");
    checkParseError("{{{{Template", "1:1:Unclosed template '{{{{Template'");
    checkParseError("Template}}", "1:9:Template closure without opening '}}'");
    checkParseError("Variable}}}", "1:9:Variable closure without opening '}}}'");
    checkParseError("Template}}}}", "1:9:Template closure without opening '}}}}'");
    checkParseError("Variatemplate}}}}}", "1:14:Template or variable closure without opening '}}}}}'");
    checkParseError("{{{{{Variatemplate", "1:1:Unclosed template or variable '{{{{{Variatemplate'");
    checkParseError("{{{Variatemplate}}", "1:1:Extra brace at template or variable opening '{{{Variatemplate}}'");
    checkParseError("{{Variatemplate}}}", "1:18:Extra brace at template or variable closure '}'");
    checkParseError("<ref>X", "1:1:Unclosed <ref> tag '<ref>X'");
    checkParseError("X</ref>", "1:2:Closing tag </ref> without opening tag '</ref>'");
    checkParseError("<!-- Comment", "1:1:Unclosed comment '<!-- Comment'");
    checkParseError("[[Link|{{]]", "1:8:Unclosed template '{{]]'");
    checkParseError("[[[Link", "1:1:Bad link opening '[[[Link'");
    // Although "]]" matches the broken link opening "[[[" so there no second warning for an unmatched "]]".
    checkParseError("[[[Link]]", "1:1:Bad link opening '[[[Link]]'");

    int oldDepth = parser_internal::setParserMaxDepth(4);
    checkParseError("{{ {{ x }} }}",
                    "1:1:Maximum parser depth reached '{{ {{ x }} }}'\n"
                    "1:1:Unclosed template '{{ {{ x }} }}'\n"
                    "1:12:Template closure without opening '}}'");
    parser_internal::setParserMaxDepth(oldDepth);

    // Do not split UTF-8 chars.
    checkParseError("[[012345678901234é*****", "1:1:Unclosed link '[[012345678901234é*...'");
    checkParseError("[[0123456789012345é****", "1:1:Unclosed link '[[0123456789012345é...'");
    checkParseError("[[01234567890123456é***", "1:1:Unclosed link '[[01234567890123456é...'");
    checkParseError("[[012345678901234567é**", "1:1:Unclosed link '[[012345678901234567...'");

    // Position
    checkParseError("a\nabc[[Link", "2:4:Unclosed link '[[Link'");

    // Multiple errors
    checkParseError("[[test<!--",
                    "1:1:Unclosed link '[[test<!--'\n"
                    "1:7:Unclosed comment '<!--'");

    // Check that the last character is parsed.
    string_view brackets = "[[[[a]]]]";
    checkParseError(brackets.substr(0, 2), "1:1:Unclosed link '[['");
    checkParseError(brackets.substr(0, 3), "1:1:Bad link opening '[[['");
    checkParseError(brackets.substr(0, 4), "1:1:Unclosed link '[[[['\n1:3:Unclosed link '[['");
    checkParseError(brackets.substr(2, 4), "1:1:Unclosed link '[[a]'");
    string_view braces = "{{{{a}}}}";
    checkParseError(braces.substr(0, 2), "1:1:Unclosed template '{{'");
    checkParseError(braces.substr(0, 3), "1:1:Unclosed variable '{{{'");
    string_view tag = "<ref>a</ref>";
    checkParseError(tag.substr(0, 5), "1:1:Unclosed <ref> tag '<ref>'");
    checkParseError(tag.substr(0, 11), "1:1:Unclosed <ref> tag '<ref>a</ref'");

    // No error
    parse("<nowiki>{{</nowiki>", STRICT);
    parse("<nowiki>{{[[}}</nowiki>", STRICT);
    parse("<nowiki><nowiki></nowiki>", STRICT);
    parse("<pre><nowiki><ref></nowiki></pre>", STRICT);
    parse("<nowiki><gallery><ref></gallery></nowiki>", STRICT);
  }
};

}  // namespace wikicode

int main() {
  wikicode::ParserTest().run();
  return 0;
}
