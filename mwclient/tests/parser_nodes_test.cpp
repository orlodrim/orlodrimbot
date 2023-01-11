#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/parser.h"
#include "parser_test_util.h"

using std::pair;
using std::string;
using std::vector;

namespace wikicode {

static TemplatePtr createTemplate(const string& code) {
  List parsedCode = parse(code);
  CBL_ASSERT(parsedCode.size() == 1 && parsedCode[0].type() == NT_TEMPLATE)
      << "Parsing '" << code << "' did not produce a single template";
  return parsedCode[0].asTemplate().copy();
}

static LinkPtr createLink(const string& code) {
  List parsedCode = parse(code);
  CBL_ASSERT(parsedCode.size() == 1 && parsedCode[0].type() == NT_LINK)
      << "Parsing '" << code << "' did not produce a single link";
  return parsedCode[0].asLink().copy();
}

class MemoryTestText : public Text {
public:
  using Text::Text;
  ~MemoryTestText() { CBL_ASSERT(destructionAllowed); }
  bool destructionAllowed = false;
};

class ParserNodesTest : public cbl::Test {
private:
  template <class T>
  static void checkSubtreeGeneric(T& range, const string& expectedSubtree) {
    string subtree;
    for (Node& node : range) {
      if (!subtree.empty()) subtree += ",";
      subtree += string(getNodeTypeString(node.type())) + "(" + node.toString() + ")";
    }
    CBL_ASSERT_EQ(subtree, expectedSubtree);
  }
  static void checkSubtree(const string& code, const string& expectedSubtree,
                           EnumerationOrder enumerationOrder = PREFIX_DFS) {
    List parsedCode = parse(code, STRICT);
    auto range = parsedCode.getNodes(enumerationOrder);
    checkSubtreeGeneric(range, expectedSubtree);
  }
  CBL_TEST_CASE(NodeSubtree) {
    // All node types, prefix DFS.
    checkSubtree("", "list()");
    checkSubtree("abc", "list(abc),text(abc)");
    checkSubtree("<!--x-->", "list(<!--x-->),comment(<!--x-->)");
    checkSubtree("<references />", "list(<references />),tag(<references />)");
    checkSubtree("<ref>x</ref>", "list(<ref>x</ref>),tag(<ref>x</ref>),list(x),text(x)");
    checkSubtree("[[x]]", "list([[x]]),link([[x]]),list(x),text(x)");
    checkSubtree("[[x|y]]", "list([[x|y]]),link([[x|y]]),list(x),text(x),list(y),text(y)");
    checkSubtree("{{x}}", "list({{x}}),template({{x}}),list(x),text(x)");
    checkSubtree("{{x|y}}", "list({{x|y}}),template({{x|y}}),list(x),text(x),list(y),text(y)");
    checkSubtree("{{{x}}}", "list({{{x}}}),var({{{x}}}),list(x),text(x)");
    checkSubtree("{{{x|y}}}", "list({{{x|y}}}),var({{{x|y}}}),list(x),text(x),list(y),text(y)");

    // All node types, postfix DFS.
    checkSubtree("", "list()", POSTFIX_DFS);
    checkSubtree("abc", "text(abc),list(abc)", POSTFIX_DFS);
    checkSubtree("{{{x|y}}}", "text(x),list(x),text(y),list(y),var({{{x|y}}}),list({{{x|y}}})", POSTFIX_DFS);

    // Complex example.
    checkSubtree("{{a|b={{c}}}} {{{d|e<!--f--><ref>Test</ref>}}}",
                 "list({{a|b={{c}}}} {{{d|e<!--f--><ref>Test</ref>}}}),"
                 "template({{a|b={{c}}}}),list(a),text(a),list(b={{c}}),text(b=),template({{c}}),list(c),text(c),"
                 "text( ),"
                 "var({{{d|e<!--f--><ref>Test</ref>}}}),"
                 "list(d),text(d),"
                 "list(e<!--f--><ref>Test</ref>),"
                 "text(e),"
                 "comment(<!--f-->),"
                 "tag(<ref>Test</ref>),list(Test),text(Test)");

    // Type filtering
    List parsedCode = parse(
        "{{stub}}\n{{Infobox|param1={{underline|value2}}|param2={{bold|{{italic|value2}}}}}}\n{{footer}}", STRICT);
    auto templates = parsedCode.getTemplates();
    checkSubtreeGeneric(templates,
                        "template({{stub}}),"
                        "template({{Infobox|param1={{underline|value2}}|param2={{bold|{{italic|value2}}}}}}),"
                        "template({{underline|value2}}),"
                        "template({{bold|{{italic|value2}}}}),"
                        "template({{italic|value2}}),"
                        "template({{footer}})");
    templates = parsedCode.getTemplates(POSTFIX_DFS);
    checkSubtreeGeneric(templates,
                        "template({{stub}}),"
                        "template({{underline|value2}}),"
                        "template({{italic|value2}}),"
                        "template({{bold|{{italic|value2}}}}),"
                        "template({{Infobox|param1={{underline|value2}}|param2={{bold|{{italic|value2}}}}}}),"
                        "template({{footer}})");

    parsedCode = parse("{{a|b={{c}}}} {{{d|e<!--f--><ref>Test</ref>}}}", STRICT);
    auto lists = parsedCode.getLists();
    checkSubtreeGeneric(lists,
                        "list({{a|b={{c}}}} {{{d|e<!--f--><ref>Test</ref>}}}),"
                        "list(a),"
                        "list(b={{c}}),"
                        "list(c),"
                        "list(d),"
                        "list(e<!--f--><ref>Test</ref>),"
                        "list(Test)");

    parsedCode = parse("[[File:A.jpg|thumb|[[This]] is the legend.]]", STRICT);
    auto links = parsedCode.getLinks();
    checkSubtreeGeneric(links, "link([[File:A.jpg|thumb|[[This]] is the legend.]]),link([[This]])");
  }

  CBL_TEST_CASE(Copy) {
    string complexCode = "abc<nowiki /><ref>{{x|y}}</ref>[[link<!-- comment -->|link2]]{{{x}}}{{{x|y}}}<references />";
    List list1 = parse(complexCode);
    string debugString1 = getNodeDebugString(list1);
    NodePtr list2 = list1.copyAsNode();
    list1.addItem("test");
    CBL_ASSERT_EQ(getNodeDebugString(*list2), debugString1);
    CBL_ASSERT_EQ(list2->toString(), complexCode);
    list1 = List();
    CBL_ASSERT_EQ(getNodeDebugString(*list2), debugString1);
    CBL_ASSERT_EQ(list2->toString(), complexCode);
  }

  CBL_TEST_CASE(ListItemsOperations) {
    List list = parse("a<!--b-->c", STRICT);
    list.addItem("d");
    list.addItem(1, "e");
    list.addItem(std::make_unique<Text>("f"));
    list.addItem(0, std::make_unique<Text>("g"));
    CBL_ASSERT_EQ(list.toString(), "gae<!--b-->cdf");

    list = parse("a<!--b-->c", STRICT);
    list.removeItem(1);
    CBL_ASSERT_EQ(list.toString(), "ac");
    list.removeItem(1);
    CBL_ASSERT_EQ(list.toString(), "a");

    list = parse("a<!--b-->", STRICT);
    list.setItem(0, "c");
    list.setItem(1, std::make_unique<Text>("d"));
    CBL_ASSERT_EQ(list.toString(), "cd");
  }

  CBL_TEST_CASE(LinkFieldsOperations) {
    // Short test, the full test of NodeWithFields is in testTemplateFieldsOperations.
    List list = parse("[[Link]]", STRICT);
    CBL_ASSERT_EQ(list[0].type(), NT_LINK);
    Link& link = list[0].asLink();
    link.addField("x");
    link.addField(1, "y");
    link.addField(1, "z");
    link.removeField(2);
    CBL_ASSERT_EQ(link.toString(), "[[Link|z|x]]");
  }

  static void checkLinkTarget(const string& code, const string& expectedTarget, const string& expectedAnchor) {
    LinkPtr link = createLink(code);
    CBL_ASSERT_EQ(link->target(), expectedTarget);
    CBL_ASSERT_EQ(link->anchor(), expectedAnchor);
  }
  CBL_TEST_CASE(LinkTarget) {
    checkLinkTarget("[[Abc]]", "Abc", "");
    checkLinkTarget("[[:Abc]]", ":Abc", "");
    checkLinkTarget("[[Abc#Def]]", "Abc", "#Def");
    checkLinkTarget("[[#Def]]", "", "#Def");
    checkLinkTarget("[[ abc <!-- test -->_ xyz  #  Def  _  <!-- test -->ghi]]", "abc xyz", "# Def ghi");
    checkLinkTarget("[[Abc#Def{{Test}}]]", "", "");
    checkLinkTarget("[[Good link#Strange&#124;anchor]]", "Good link", "#Strange|anchor");
    checkLinkTarget("[[Bad&#124;link]]", "", "");
  }

  CBL_TEST_CASE(TemplateFieldsOperations) {
    TemplatePtr template_ = createTemplate("{{Test}}");
    template_->addField("1");
    template_->addField(List("2"));
    template_->addField(3, "3");
    template_->addField(4, List("4"));
    template_->addField(0, "5");
    template_->addField(0, List("6"));
    CBL_ASSERT_EQ(template_->toString(), "{{6|5|Test|1|2|3|4}}");

    template_ = createTemplate("{{Test|a|b|c|d|e}}");
    template_->removeField(5);
    CBL_ASSERT_EQ(template_->toString(), "{{Test|a|b|c|d}}");
    template_->removeField(0);
    CBL_ASSERT_EQ(template_->toString(), "{{a|b|c|d}}");
    template_->removeField(3);
    CBL_ASSERT_EQ(template_->toString(), "{{a|b|c}}");
    template_->removeField(1);
    CBL_ASSERT_EQ(template_->toString(), "{{a|c}}");

    template_ = createTemplate("{{Test|[[a]]}}");
    template_->setField(0, "x");
    template_->setField(1, List("y"));
    CBL_ASSERT_EQ(template_->toString(), "{{x|y}}");

    CBL_ASSERT_EQ(createTemplate("{{Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{Test # anchor}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{Test%40}}")->name(), "Test%40");
    CBL_ASSERT_EQ(createTemplate("{{T&#101;st&#35;anchor}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{:Test}}")->name(), ":Test");
    CBL_ASSERT_EQ(createTemplate("{{ _ x <!-- comment -->__ y _ \n}}")->name(), "x y");
    CBL_ASSERT_EQ(createTemplate("{{ x/{{{y}}} }}")->name(), "");
    CBL_ASSERT_EQ(createTemplate("{{#if:1}}")->name(), "#if:1");

    CBL_ASSERT_EQ(createTemplate("{{subst:Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{safesubst:Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{ subst: Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{{{{|subst:}}}Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{{{{|safesubst:}}}Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{{{{|safesubst:<!-- comment -->}}}Test}}")->name(), "Test");
    CBL_ASSERT_EQ(createTemplate("{{{{{x|safesubst:}}}Test}}")->name(), "");
    CBL_ASSERT_EQ(createTemplate("{{ {{{|safesubst:}}} #invoke:Abc}}")->name(), "#invoke:Abc");

    template_ = createTemplate("{{Test|a|b|c}}");
    template_->removeAllFieldsExceptFirst();
    CBL_ASSERT_EQ(template_->toString(), "{{Test}}");
  }

  static void checkSplitParamValue(const Template& template_, int fieldIndex, int flags, const char* expectedParam,
                                   const char* expectedValue) {
    string actualParam, actualValue;
    template_.splitParamValue(fieldIndex, expectedParam ? &actualParam : nullptr,
                              expectedValue ? &actualValue : nullptr, flags);
    if (expectedParam != nullptr) {
      CBL_ASSERT_EQ(actualParam, expectedParam) << template_.toString() << " fieldIndex=" << fieldIndex;
    }
    if (expectedValue != nullptr) {
      CBL_ASSERT_EQ(actualValue, expectedValue) << template_.toString() << " fieldIndex=" << fieldIndex;
    }
  }
  CBL_TEST_CASE(TemplateSplitParamValue) {
    TemplatePtr template_ = createTemplate("{{Test|param1=value1|param2=value2}}");
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, "param1", "value1");
    checkSplitParamValue(*template_, 2, NORMALIZE_PARAM, "param2", "value2");
    template_ = createTemplate(
        "{{Test\n"
        " | param1 = value1\n"
        " | param__2  2 = value2\n"
        "}}");
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, "param1", " value1\n ");
    checkSplitParamValue(*template_, 2, NORMALIZE_PARAM, "param__2 2", " value2\n");
    template_ = createTemplate("{{Test| a <!-- comment --> b = c{{d}} | e{{f}} |=g}}");
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, nullptr, nullptr);
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, "a b", nullptr);
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, nullptr, " c{{d}} ");
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, "a b", " c{{d}} ");
    checkSplitParamValue(*template_, 1, 0, " a <!-- comment --> b ", nullptr);
    checkSplitParamValue(*template_, 1, 0, nullptr, " c{{d}} ");
    checkSplitParamValue(*template_, 1, 0, " a <!-- comment --> b ", " c{{d}} ");
    checkSplitParamValue(*template_, 2, NORMALIZE_PARAM, UNNAMED_PARAM, nullptr);
    checkSplitParamValue(*template_, 2, NORMALIZE_PARAM, nullptr, " e{{f}} ");
    checkSplitParamValue(*template_, 2, NORMALIZE_PARAM, UNNAMED_PARAM, " e{{f}} ");
    checkSplitParamValue(*template_, 3, NORMALIZE_PARAM, "", nullptr);
    checkSplitParamValue(*template_, 3, NORMALIZE_PARAM, nullptr, "g");
    checkSplitParamValue(*template_, 3, NORMALIZE_PARAM, "", "g");

    // https://fr.wikipedia.org/w/index.php?diff=148210032
    template_ = createTemplate("{{Test|=1|==2|\n=3=\n|\n==4|\n==5==\n|\n<!--test-->==6==|=|\n=}}");
    checkSplitParamValue(*template_, 1, NORMALIZE_PARAM, "", "1");
    checkSplitParamValue(*template_, 2, NORMALIZE_PARAM, "", "=2");
    checkSplitParamValue(*template_, 3, NORMALIZE_PARAM, "", "3=\n");
    checkSplitParamValue(*template_, 4, NORMALIZE_PARAM, UNNAMED_PARAM, "\n==4");
    checkSplitParamValue(*template_, 5, NORMALIZE_PARAM, UNNAMED_PARAM, "\n==5==\n");
    checkSplitParamValue(*template_, 6, NORMALIZE_PARAM, "", "=6==");
    checkSplitParamValue(*template_, 7, NORMALIZE_PARAM, "", "");
    checkSplitParamValue(*template_, 8, NORMALIZE_PARAM, "", "");

    // Value normalization
    template_ = createTemplate("{{Test\n| first  value  <!-- test -->\n}}");
    checkSplitParamValue(*template_, 1, 0, UNNAMED_PARAM, " first  value  <!-- test -->\n");
    checkSplitParamValue(*template_, 1, TRIM_VALUE, UNNAMED_PARAM, "first  value  <!-- test -->");
    checkSplitParamValue(*template_, 1, TRIM_AND_COLLAPSE_SPACE_IN_VALUE, UNNAMED_PARAM, "first value <!-- test -->");
    checkSplitParamValue(*template_, 1, STRIP_COMMENTS_IN_VALUE, UNNAMED_PARAM, " first  value  \n");
    checkSplitParamValue(*template_, 1, TRIM_VALUE | STRIP_COMMENTS_IN_VALUE, UNNAMED_PARAM, "first  value");
    checkSplitParamValue(*template_, 1, NORMALIZE_COLLAPSE_VALUE, UNNAMED_PARAM, "first value");
  }

  CBL_TEST_CASE(TemplateFieldMutation) {
    TemplatePtr template_ = createTemplate("{{Test|x=1|\n y z = 2 |=3| = 4|5| 6 }}");
    template_->setFieldName(1, "a");
    template_->setFieldName(2, "b");
    template_->setFieldName(3, "c");
    template_->setFieldName(4, "d");
    template_->setFieldName(5, "e");
    template_->setFieldName(6, "f");
    CBL_ASSERT_EQ(template_->toString(), "{{Test|a=1|\n b = 2 |c=3|d = 4|e=5|f= 6 }}");

    template_ = createTemplate("{{Test|v| v|v | v |  |p=|p=v| p =  | p =\n | = v|p =  vvv\n}}");
    template_->setFieldValue(1, "V");
    template_->setFieldValue(2, "VVV");
    template_->setFieldValue(3, "V");
    template_->setFieldValue(4, "V");
    template_->setFieldValue(5, "V");
    template_->setFieldValue(6, "V");
    template_->setFieldValue(7, "V");
    template_->setFieldValue(8, "V");
    template_->setFieldValue(9, "V");
    template_->setFieldValue(10, "V");
    template_->setFieldValue(11, "V");
    CBL_ASSERT_EQ(template_->toString(), "{{Test|V| VVV|V | V | V |p=V|p=V| p = V | p =V\n | = V|p =  V\n}}");
  }

  static void checkGetParsedFields(const string& code, const string& expectedParsedFields) {
    ParsedFields parsedFields = createTemplate(code)->getParsedFields();
    vector<const TemplateField*> sortedFields;
    for (const TemplateField& templateField : parsedFields) {
      sortedFields.push_back(&templateField);
    }
    std::sort(sortedFields.begin(), sortedFields.end(),
              [](const TemplateField* field1, const TemplateField* field2) { return field1->param < field2->param; });
    string parsedFieldsStr;
    for (const TemplateField* field : sortedFields) {
      if (!parsedFieldsStr.empty()) parsedFieldsStr += ',';
      parsedFieldsStr += field->param + "=>" + field->value;
    }
    CBL_ASSERT_EQ(parsedFieldsStr, expectedParsedFields) << code;
  }
  static void checkGetParsedFieldsOrdered(const string& code, const string& expectedParsedFields) {
    ParsedFields parsedFields = createTemplate(code)->getParsedFields();
    string parsedFieldsStr;
    for (const TemplateField& field : parsedFields.orderedFields()) {
      if (!parsedFieldsStr.empty()) parsedFieldsStr += ',';
      parsedFieldsStr += field.param + "=>" + field.value;
    }
    CBL_ASSERT_EQ(parsedFieldsStr, expectedParsedFields) << code;
  }
  CBL_TEST_CASE(getParsedFields) {
    // Unnamed fields
    checkGetParsedFields("{{Test|red|green|blue}}", "1=>red,2=>green,3=>blue");
    // Named fields
    checkGetParsedFields("{{Test|color1=red|color2=green|color3=blue}}", "color1=>red,color2=>green,color3=>blue");
    // More complex cases
    checkGetParsedFields("{{Test|color1=red|green|2=blue=orange}}", "1=>green,2=>blue=orange,color1=>red");
    checkGetParsedFields("{{Test|a|=b|param1=c|d| param2 =e}}", "=>b,1=>a,2=>d,param1=>c,param2=>e");
    checkGetParsedFieldsOrdered("{{Test|a|=b|param1=c|d| param2 =e}}", "1=>a,=>b,param1=>c,2=>d,param2=>e");
    // Duplicate field
    checkGetParsedFields("{{Test|color1=red|color1=blue}}", "color1=>blue");
    checkGetParsedFieldsOrdered("{{Test|color1=red|color1=blue}}", "color1=>red,color1=>blue");
    // Spaces and comments
    checkGetParsedFields(
        "{{Test\n"
        " | color1 = red <!-- some comment -->\n"
        " | green\n"
        " | color3 <!-- some other comment --> = blue\n"
        "}}",
        "1=>green,color1=>red,color3=>blue");

    ParsedFields parsedFields = createTemplate("{{Test|color1=red|color2=blue}}")->getParsedFields();
    CBL_ASSERT(parsedFields.contains("color1"));
    CBL_ASSERT(parsedFields.contains("color2"));
    CBL_ASSERT(!parsedFields.contains("color3"));
    CBL_ASSERT_EQ(parsedFields["color1"], "red");
    CBL_ASSERT_EQ(parsedFields["color2"], "blue");
    CBL_ASSERT_EQ(parsedFields["color3"], "");
    CBL_ASSERT_EQ(parsedFields.getWithDefault("color1", "other"), "red");
    CBL_ASSERT_EQ(parsedFields.getWithDefault("color2", "other"), "blue");
    CBL_ASSERT_EQ(parsedFields.getWithDefault("color3", "other"), "other");
    CBL_ASSERT_EQ(parsedFields.indexOf("color1"), 1);
    CBL_ASSERT_EQ(parsedFields.indexOf("color2"), 2);
    CBL_ASSERT_EQ(parsedFields.indexOf("color3"), FIND_PARAM_NONE);
  }

  CBL_TEST_CASE(NodeGenerator) {
    vector<pair<NodeType, string>> expectedNodes = {
        {NT_LIST, "{{template|x=[[link]]}}"},
        {NT_TEMPLATE, "{{template|x=[[link]]}}"},
        {NT_LIST, "template"},
        {NT_TEXT, "template"},
        {NT_LIST, "x=[[link]]"},
        {NT_TEXT, "x="},
        {NT_LINK, "[[link]]"},
        {NT_LIST, "link"},
        {NT_TEXT, "link"},
    };
    List root = parse("{{template|x=[[link]]}}");
    NodeGenerator generator(&root);
    for (const auto& [type, code] : expectedNodes) {
      CBL_ASSERT(generator.next());
      CBL_ASSERT_EQ(generator.value().type(), type) << code;
      CBL_ASSERT_EQ(generator.value().toString(), code);
      if (type == NT_LINK) {
        CBL_ASSERT(generator.parent() != nullptr);
        CBL_ASSERT_EQ(generator.parent()->toString(), "x=[[link]]");
        CBL_ASSERT_EQ(generator.indexInParent(), 1);
        CBL_ASSERT(generator.ancestor(0) == &generator.value());
        CBL_ASSERT(generator.ancestor(1) == generator.parent());
        CBL_ASSERT(generator.ancestor(2) != nullptr);
        CBL_ASSERT_EQ(generator.ancestor(2)->type(), NT_TEMPLATE);
        CBL_ASSERT_EQ(generator.ancestor(2)->toString(), "{{template|x=[[link]]}}");
        CBL_ASSERT_EQ(generator.indexInAncestor(2), 1);
        CBL_ASSERT(generator.ancestor(3) != nullptr);
        CBL_ASSERT_EQ(generator.ancestor(3)->type(), NT_LIST);
        CBL_ASSERT_EQ(generator.ancestor(3)->toString(), "{{template|x=[[link]]}}");
        CBL_ASSERT_EQ(generator.indexInAncestor(3), 0);
        CBL_ASSERT(generator.ancestor(4) == nullptr);
        CBL_ASSERT_EQ(generator.indexInAncestor(4), 0);
      }
    }
    CBL_ASSERT(!generator.next());
  }

  CBL_TEST_CASE(MemoryManagement) {
    // The previous content of replaced list items can be kept in a buffer so that it is not destructed immediatly.
    {
      MemoryTestText* text = new MemoryTestText("test");
      List list;
      list.addItem(NodePtr(text));
      vector<NodePtr> deletedItems;
      deletedItems.push_back(list.setItem(0, "test2"));
      text->destructionAllowed = true;
    }
    // Changing nodes during iteration does not cause iteration on destructed nodes.
    {
      List root = parse("{{eraseme|{{test}}|{{test2}}}} {{test3|{{eraseme|{{test4}}}}|{{test5}}}}", STRICT);
      string templatesProcessed;
      for (Template& template_ : root.getTemplates()) {
        template_.addToBuffer(templatesProcessed);
        templatesProcessed += ',';
        if (template_.name() == "eraseme") {
          template_.removeAllFieldsExceptFirst();
        }
      }
      CBL_ASSERT_EQ(templatesProcessed,
                    "{{eraseme|{{test}}|{{test2}}}},"
                    "{{test3|{{eraseme|{{test4}}}}|{{test5}}}},"
                    "{{eraseme|{{test4}}}},"
                    "{{test5}},");
    }
  }
};

}  // namespace wikicode

int main() {
  wikicode::ParserNodesTest().run();
  return 0;
}
