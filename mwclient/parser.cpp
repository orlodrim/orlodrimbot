#include "parser.h"
#include <ctype.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/log.h"
#include "parser_nodes.h"

using std::queue;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace wikicode {
namespace parser_internal {

// The parser currently supports two ways of parsing tags.
// - RAW_TAG: the content of the tag is stored in a single text node.
// - WIKICODE_TAG: the content is parsed as normal wikicode.
// This is simplified compared to how MediaWiki works. In reality, each tag can parse its content in completely
// arbitrary ways. However, this is good enough to allow processing within common tags such as <ref> or <gallery>.
enum ParserExtensionTagContent {
  RAW_TAG,
  WIKICODE_TAG,
};

const unordered_map<string, ParserExtensionTagContent> PARSER_EXTENSION_TAGS = {
    {"categorytree", RAW_TAG},
    {"ce", RAW_TAG},  // ce = Chemical element (https://gerrit.wikimedia.org/r/#/c/267241/)
    {"chem", RAW_TAG},
    {"gallery", WIKICODE_TAG},  // TODO: Parse by line.
    {"graph", RAW_TAG},
    {"hiero", RAW_TAG},
    {"imagemap", WIKICODE_TAG},
    {"indicator", WIKICODE_TAG},
    {"inputbox", WIKICODE_TAG},
    {"mapframe", RAW_TAG},       // Can contain wikicode but within JSON values (so it is JSON-escaped).
    {"maplink", WIKICODE_TAG},
    {"math", RAW_TAG},
    {"nowiki", RAW_TAG},
    {"poem", WIKICODE_TAG},
    {"pre", RAW_TAG},
    {"ref", WIKICODE_TAG},
    {"references", WIKICODE_TAG},  // There is a non-autoclosed form with named <ref> tags inside <references>.
    {"score", RAW_TAG},
    {"section", WIKICODE_TAG},
    {"source", RAW_TAG},
    {"syntaxhighlight", RAW_TAG},
    {"templatedata", RAW_TAG},
    {"templatestyles", RAW_TAG},
    {"timeline", RAW_TAG},
};

enum ParserWarnings {
  MISSING_LINK_CLOSURE = 1,
  MISSING_LINK_OPENING = 2,
  BAD_LINK_OPENING = 4,
  LINK_WITH_LINE_BREAK = 8,
  MISSING_TEMPLATE_CLOSURE = 0x10,
  MISSING_TEMPLATE_OPENING = 0x20,
  MISSING_TAG_CLOSURE = 0x40,
  MISSING_TAG_OPENING = 0x80,
  MISSING_COMMENT_CLOSURE = 0x100,
  MAX_DEPTH_REACHED = 0x200,

  ALL_WARNINGS = 0xFFFF,
};

struct CharRange {
  bool empty() const { return begin == end; }
  int size() const { return end - begin; }
  const char* begin;
  const char* end;
};

class WarningsBuffer {
public:
  explicit WarningsBuffer(const char* codeBegin, const char* codeEnd, int enabledWarnings)
      : m_codeBegin(codeBegin), m_codeEnd(codeEnd), m_enabledWarnings(enabledWarnings) {}
  void add(int type, const char* position, const string& message);
  bool empty() const { return m_warnings.empty(); }
  string toString() const;
  int enabledWarnings() const { return m_enabledWarnings; }

private:
  struct Warning {
    const char* position;
    string message;
  };

  const char* m_codeBegin = nullptr;
  const char* m_codeEnd = nullptr;
  int m_enabledWarnings = 0;
  vector<Warning> m_warnings;
};

void WarningsBuffer::add(int type, const char* position, const string& message) {
  if (!(m_enabledWarnings & type)) return;
  m_warnings.emplace_back();
  Warning& warning = m_warnings.back();
  warning.position = position;
  warning.message = message;
}

string WarningsBuffer::toString() const {
  vector<const Warning*> warnings;
  warnings.reserve(m_warnings.size());
  for (const Warning& warning : m_warnings) {
    warnings.push_back(&warning);
  }
  std::sort(warnings.begin(), warnings.end(), [](const Warning* warning1, const Warning* warning2) {
    if (warning1->position != warning2->position) {
      return warning1->position < warning2->position;
    }
    return warning1 < warning2;  // Stable sort.
  });
  string text;
  int lineNumber = 1, columnNumber = 1;
  const char* p = m_codeBegin;
  for (const Warning* warning : warnings) {
    CBL_ASSERT(warning->position >= m_codeBegin && warning->position <= m_codeEnd);
    for (; p < warning->position; p++) {
      columnNumber++;
      if (*p == '\n') {
        lineNumber++;
        columnNumber = 1;
      }
    }
    if (!text.empty()) {
      text += '\n';
    }
    text += std::to_string(lineNumber) + ':' + std::to_string(columnNumber) + ':' + warning->message + " '";
    for (const char* context = p; context < m_codeEnd; context++) {
      if (context >= p + 20 && (*context & 0xC0) != 0x80) {  // Only cut full UTF-8 characters.
        text += "...";
        break;
      }
      text += *context == '\n' ? ' ' : *context;
    }
    text += "'";
  }
  return text;
}

enum TagType {
  OPENING_TAG,
  CLOSING_TAG,
  SELF_CLOSING_TAG,
};

static bool parseTagNameAndType(const char*& position, const char* codeEnd, string& tagName, TagType& tagType) {
  const char* p = position;
  if (p > codeEnd - 2 || *p != '<') return false;
  // Now, p <= codeEnd - 2.
  p++;
  // Now, p <= codeEnd - 1.
  tagType = *p == '/' ? CLOSING_TAG : OPENING_TAG;
  if (*p == '/') p++;
  tagName.clear();
  for (; p < codeEnd && isalnum(static_cast<unsigned char>(*p)); p++) {
    tagName += *p + (*p >= 'A' && *p <= 'Z' ? 'a' - 'A' : 0);
  }
  if (PARSER_EXTENSION_TAGS.count(tagName) == 0) return false;
  if (p >= codeEnd || (*p != ' ' && *p != '/' && *p != '>')) return false;
  for (; p < codeEnd && *p != '<' && *p != '>'; p++) {
  }
  if (p >= codeEnd || *p != '>') return false;
  // Now, p <= codeEnd - 1;
  if (tagType == OPENING_TAG && *(p - 1) == '/') {
    tagType = SELF_CLOSING_TAG;
  }
  position = p + 1;
  // Now, position <= codeEnd.
  return true;
}

// Find closing tags in amortized linear time.
// MediaWiki shows most tags as plain text if they do not have a corresponding closing tag. Since parsing the text after
// the tag can be very different depending on whether we are inside the tag or not (e.g. links and templates are not
// parsed inside <nowiki>), we need to know whether there is a closing tag at the moment we see the opening tag.
// We could do a preliminary pass over the code to find them all. This class basically does this, except that it avoids
// doing a complete pass when all tags are properly closed (any code that is not inside at least one tag is skipped).
class ClosingTagFinder {
public:
  ClosingTagFinder(const char* codeBegin, const char* codeEnd)
      : m_lastRequestPosition(codeBegin), m_parsingPosition(codeBegin), m_codeEnd(codeEnd) {}

  // Finds the first occurrence of the tag tagName from position start.
  // Successive calls must have non-decreasing values for start.
  CharRange findClosingTag(const string& tagName, const char* start);

private:
  const char* m_lastRequestPosition;
  const char* m_parsingPosition;
  const char* m_codeEnd;
  unordered_map<string, queue<CharRange>> m_closingTagsByName;
};

CharRange ClosingTagFinder::findClosingTag(const string& tagName, const char* start) {
  CBL_ASSERT(start >= m_lastRequestPosition && start <= m_codeEnd);
  queue<CharRange>& tagRanges = m_closingTagsByName[tagName];
  for (; !tagRanges.empty() && tagRanges.front().begin < start; tagRanges.pop())
    ;
  const char* p = std::max(m_parsingPosition, start);

  // Parse until we reach the first closing tag for tagName or the end, filling m_closingTagsByName for all tags.
  string loopTagName;
  TagType loopTagType;
  while (tagRanges.empty()) {
    p = static_cast<const char*>(memchr(p, '<', m_codeEnd - p));
    if (!p) {
      p = m_codeEnd;
      break;
    }
    const char* tagBegin = p;
    if (parseTagNameAndType(p, m_codeEnd, loopTagName, loopTagType)) {
      // p has been increase by parseTagNameAndType but is still <= m_codeEnd.
      if (loopTagType == CLOSING_TAG) {
        m_closingTagsByName[loopTagName].push({tagBegin, p});
      }
    } else {
      p++;
    }
  }

  CharRange result = {};
  if (!tagRanges.empty()) {
    result = tagRanges.front();
    CBL_ASSERT(result.begin >= start);
  }
  m_parsingPosition = p;
  m_lastRequestPosition = start;
  return result;
}

enum StackElementType {
  NODE_ELEMENT,
  TOKEN_PLAIN_TEXT,
  TOKEN_LINK_BEGIN,               // "[["
  TOKEN_LINK_BROKEN_BEGIN,        // "[[["
  TOKEN_LINK_END,                 // "]]"
  TOKEN_TEMPLATE_BEGIN,           // 2 or more "{" (despite the name, this also covers variables)
  TOKEN_TEMPLATE_BEGIN_LEFTOVER,  // "{"
  TOKEN_TEMPLATE_END,             // Any number of "}" (also covers variables)
  TOKEN_PIPE,                     // "|"
};

class ParserStack {
public:
  class Element {
  public:
    Element(Node* node, int depth) : type(NODE_ELEMENT), node(node), depth(depth) {}
    Element(StackElementType type, const char* begin, const char* end) : type(type), range{begin, end} {}
    Element(const Element&) = delete;
    Element(Element&& element) : type(element.type) {
      if (type == NODE_ELEMENT) {
        node = element.node;
        depth = element.depth;
        element.node = nullptr;
      } else {
        range = element.range;
      }
    }
    ~Element() {
      if (type == NODE_ELEMENT && node) {
        delete node;
      }
    }
    Element& operator=(const Element&) = delete;
    Element& operator=(Element&& element) {
      if (this != &element) {
        if (type == NODE_ELEMENT && node) {
          delete node;
        }
        type = element.type;
        if (type == NODE_ELEMENT) {
          node = element.node;
          depth = element.depth;
          element.node = nullptr;
        } else {
          range = element.range;
        }
      }
      return *this;
    }
    Node* release() {
      CBL_ASSERT(type == NODE_ELEMENT);
      Node* oldNode = node;
      node = nullptr;
      return oldNode;
    }

    StackElementType type;
    union {
      CharRange range;
      struct {
        Node* node;
        int depth;
      };
    };
  };

  static int setParserMaxDepth(int maxDepth) {
    int oldValue = PARSER_MAX_DEPTH;
    PARSER_MAX_DEPTH = maxDepth;
    return oldValue;
  }

  bool maxDepthReached() const { return m_maxDepthReached; }
  void pushElement(Element&& element) {
    m_elements.push_back(std::move(element));
    updateOpeningElementsAfterInsertion();
  }
  void pushNode(unique_ptr<Node> node, int depth) {
    m_elements.emplace_back(node.release(), depth);
    updateOpeningElementsAfterInsertion();
  }
  void pushToken(StackElementType type, const char* begin, const char* end) {
    CBL_ASSERT(type != NODE_ELEMENT);
    m_elements.emplace_back(type, begin, end);
    updateOpeningElementsAfterInsertion();
  }
  Element pop() {
    CBL_ASSERT(!m_elements.empty());
    Element lastElement = std::move(m_elements.back());
    m_elements.pop_back();
    updateOpeningElementsAfterRemoval();
    return lastElement;
  }
  void popMany(int newStackEnd) {
    CBL_ASSERT(newStackEnd <= size());
    m_elements.erase(m_elements.begin() + newStackEnd, m_elements.end());
    updateOpeningElementsAfterRemoval();
  }
  bool empty() const { return m_elements.empty(); }
  int size() const { return m_elements.size(); }
  const Element& operator[](int index) const {
    CBL_ASSERT(index >= 0 && index < size());
    return m_elements[index];
  }
  const Element& back() const {
    CBL_ASSERT(!m_elements.empty());
    return m_elements.back();
  }
  // Having this function allows operator[] to be const, so that it is easier to find places that modify the stack.
  unique_ptr<Node> extractNodeFromElement(int index) {
    CBL_ASSERT(index >= 0 && index < size() && m_elements[index].node != nullptr);
    return unique_ptr<Node>(m_elements[index].release());
  }
  int getLastLinkOpening(bool skipTemplates = false) const {
    return skipTemplates || m_linkOpenings.back() > m_templateOpenings.back() ? m_linkOpenings.back() : -1;
  }
  int getLastTemplateOpening(bool skipLinks = false) const {
    return skipLinks || m_templateOpenings.back() > m_linkOpenings.back() ? m_templateOpenings.back() : -1;
  }
  void dropLinkBrokenOpening() {
    CBL_ASSERT(m_linkOpenings.back() != -1 && m_elements[m_linkOpenings.back()].type == TOKEN_LINK_BROKEN_BEGIN);
    m_linkOpenings.pop_back();
  }

  string debugString() const {
    string str;
    str += '[';
    int notFirst = false;
    for (const Element& element : m_elements) {
      if (notFirst) str += ", ";
      if (element.type == NODE_ELEMENT) {
        if (element.node) {
          str += "N\"";
          element.node->addToBuffer(str);
          str += "\"";
        } else {
          str += "N(null)";
        }
      } else {
        str += "T" + std::to_string(element.type) + "\"";
        str.append(element.range.begin, element.range.end);
        str += "\"";
      }
      notFirst = true;
    }
    str += "]";
    return str;
  }

private:
  // Maximum depth of constructed links, templates and variables. Due to tags, it can be exceeded a bit, but since
  // MediaWiki does not support nested identical tags, it only adds about 2 * PARSER_EXTENSION_TAGS.size() to the limit.
  // The parsing itself is not recursive, so it could construct very deep nodes, but then Node::~Node() could cause a
  // stack overflow.
  static int PARSER_MAX_DEPTH;

  void updateOpeningElementsAfterInsertion() {
    switch (m_elements.back().type) {
      case NODE_ELEMENT:
        if (m_elements.back().depth >= PARSER_MAX_DEPTH - 1) {
          m_linkOpenings.resize(1);
          m_templateOpenings.resize(1);
          m_maxDepthReached = true;
        }
        break;
      case TOKEN_LINK_BEGIN:
      case TOKEN_LINK_BROKEN_BEGIN:
        m_linkOpenings.push_back(m_elements.size() - 1);
        break;
      case TOKEN_TEMPLATE_BEGIN:
        m_templateOpenings.push_back(m_elements.size() - 1);
        break;
      default:
        break;
    }
  }
  void updateOpeningElementsAfterRemoval() {
    int size = m_elements.size();
    for (; m_linkOpenings.back() >= size; m_linkOpenings.pop_back()) {
    }
    for (; m_templateOpenings.back() >= size; m_templateOpenings.pop_back()) {
    }
  }
  vector<Element> m_elements;
  // -1 acts as a sentinel value for updateOpeningElementsAfterRemoval and is never removed.
  vector<int> m_linkOpenings = {-1};
  vector<int> m_templateOpenings = {-1};
  bool m_maxDepthReached = false;
};

int ParserStack::PARSER_MAX_DEPTH = 1000;

class CodeParser {
public:
  explicit CodeParser(const char* codeBegin, const char* codeEnd, WarningsBuffer* warningsBuffer,
                      ClosingTagFinder* closingTagFinder)
      : m_position(codeBegin), m_codeEnd(codeEnd), m_warningsBuffer(warningsBuffer),
        m_closingTagFinder(closingTagFinder) {}
  List parse();
  int totalDepth() const { return m_totalDepth; }

private:
  // == Lexer ==
  // Parses a comment, assuming that the code at m_position starts with "<!--".
  void parseComment();
  // Parses a tag (including its content).
  bool parseTag();
  // Parses a token from code at m_position and pushes it on m_stack.
  bool parseToken();

  // == Construction of nodes ==
  // Constructs a List from elements between index and the end of the stack, or the first '|' if stopOnPipe is true.
  // The elements used to build the List are left on the stack, but in an undefined state.
  List constructList(int& index, int& depth, bool stopOnPipe);
  // Same as above, but takes index by value and never stops on '|'.
  List constructList(int beginIndex, int& depth) { return constructList(beginIndex, depth, false); }
  // The stack elements used to build the node are left in an undefined state.
  void constructNodeWithFields(int beginIndex, int& depth, NodeWithFields& node);
  void reduceLink();
  void reduceTemplateOrVariable();
  void reduce();
  // The parser should behave as if it parsed templates and variables first, and then parsed links, ignoring any
  // unmatched "{{" left during the first pass.
  // Instead of always doing two passes, we try to do everything at once when everything is balanced, but fallback to
  // doing a second pass for links when both unmatched "[[" and unmatched "{{" remain. reparseLinksIfNeeded() detects
  // this situation and does the second pass.
  void reparseLinksIfNeeded(int beginIndex);

  const char* m_position = nullptr;
  const char* m_codeEnd = nullptr;
  WarningsBuffer* m_warningsBuffer = nullptr;
  ClosingTagFinder* m_closingTagFinder = nullptr;
  ParserStack m_stack;
  int m_totalDepth = 0;
};

void CodeParser::parseComment() {
  const char* commentEnd = nullptr;
  for (const char* p = m_position + 4; p <= m_codeEnd - 3; p++) {
    if (p[0] == '-' && p[1] == '-' && p[2] == '>') {
      commentEnd = p + 3;
      break;
    }
  }
  if (!commentEnd) {
    m_warningsBuffer->add(MISSING_COMMENT_CLOSURE, m_position, "Unclosed comment");
    commentEnd = m_codeEnd;
  }
  unique_ptr<Comment> comment = std::make_unique<Comment>();
  comment->text.assign(m_position, commentEnd);
  m_stack.pushNode(std::move(comment), 1);
  m_position = commentEnd;
}

bool CodeParser::parseTag() {
  string tagName;
  TagType tagType;
  const char* tagEnd = m_position;
  if (!parseTagNameAndType(tagEnd, m_codeEnd, tagName, tagType)) return false;
  if (tagType == CLOSING_TAG) {
    m_warningsBuffer->add(MISSING_TAG_OPENING, m_position,
                          "Closing tag " + string(m_position, tagEnd) + " without opening tag");
    return false;
  }

  unique_ptr<Tag> tag = std::make_unique<Tag>();
  tag->setTagName(tagName);
  tag->setOpeningTag(string_view(m_position, tagEnd - m_position));
  int innerDepth = 0;

  if (tagType == OPENING_TAG) {
    CharRange closingTag = m_closingTagFinder->findClosingTag(tagName, tagEnd);
    if (!closingTag.empty() && closingTag.end <= m_codeEnd) {
      tag->setClosingTag(string_view(closingTag.begin, closingTag.end - closingTag.begin));
    } else {
      m_warningsBuffer->add(MISSING_TAG_CLOSURE, m_position, "Unclosed " + tag->openingTag() + " tag");
      // Most tags require a closing tag, but <pre> does not.
      if (tagName != "pre") {
        return false;
      }
      closingTag = CharRange{m_codeEnd, m_codeEnd};
    }
    switch (PARSER_EXTENSION_TAGS.at(tagName)) {
      case RAW_TAG:
        tag->mutableContent() = List(string(tagEnd, closingTag.begin));
        innerDepth = tag->content()->empty() ? 1 : 2;
        break;
      case WIKICODE_TAG: {
        // To make parsing work in linear time, it is important to use the same ClosingTagFinder for the tag content.
        // The constraint that successive calls of findClosingTag must have non-decreasing values for start is fulfilled
        // because:
        // - The previous call is done just above with start = tagEnd.
        // - All calls done by tagContentParser will have tagEnd <= start <= closingTag.begin.
        // - At this level, the next call to parseTag will be with m_position >= closingTag.end >= closingTag.begin.
        CodeParser tagContentParser(tagEnd, closingTag.begin, m_warningsBuffer, m_closingTagFinder);
        tag->mutableContent() = tagContentParser.parse();
        innerDepth = tagContentParser.totalDepth();
        break;
      }
    }
    m_position = closingTag.end;
  } else {
    m_position = tagEnd;
  }

  m_stack.pushNode(std::move(tag), innerDepth + 1);
  return true;
}

bool CodeParser::parseToken() {
  if (m_position >= m_codeEnd) {
    return false;
  }
  const char* tokenBegin = m_position;
  switch (*tokenBegin) {
    case '<':
      if (m_position + 3 < m_codeEnd && tokenBegin[1] == '!' && tokenBegin[2] == '-' && tokenBegin[3] == '-') {
        parseComment();
        return true;
      } else if (parseTag()) {
        return true;
      }
      break;
    case '[':
      if (m_position + 1 < m_codeEnd && tokenBegin[1] == '[') {
        if (m_position + 2 < m_codeEnd && tokenBegin[2] == '[' &&
            !(m_position + 3 < m_codeEnd && tokenBegin[3] == '[')) {
          m_position += 3;
          m_stack.pushToken(TOKEN_LINK_BROKEN_BEGIN, tokenBegin, m_position);
        } else {
          m_position += 2;
          m_stack.pushToken(TOKEN_LINK_BEGIN, tokenBegin, m_position);
        }
        return true;
      }
      break;
    case '{':
      if (m_position + 1 < m_codeEnd && tokenBegin[1] == '{') {
        for (m_position += 2; m_position < m_codeEnd && *m_position == '{'; m_position++) {
        }
        m_stack.pushToken(TOKEN_TEMPLATE_BEGIN, tokenBegin, m_position);
        return true;
      }
      break;
    case ']':
      if (m_position + 1 < m_codeEnd && tokenBegin[1] == ']') {
        m_position += 2;
        m_stack.pushToken(TOKEN_LINK_END, tokenBegin, m_position);
        return true;
      }
      break;
    case '}':
      if (m_position + 1 < m_codeEnd && tokenBegin[1] == '}') {
        for (m_position += 2; m_position < m_codeEnd && *m_position == '}'; m_position++) {
        }
        m_stack.pushToken(TOKEN_TEMPLATE_END, tokenBegin, m_position);
        return true;
      }
      break;
    case '|':
      m_position++;
      m_stack.pushToken(TOKEN_PIPE, tokenBegin, m_position);
      return true;
  }
  // We know that we can generate a plain text token of at least one char. Also consume everything after that char is
  // certainly not part of a special token.
  // This does not always produce the longest possible plain text tokens between other types of tokens. For instance,
  // the lexer splits "abc{def}" into ["abc", "{def", "}"]. However, this does not matter since those plain text tokens
  // are concatenated by constructList.
  for (m_position++; m_position < m_codeEnd; m_position++) {
    switch (*m_position) {
      case '<':
      case '[':
      case '{':
      case ']':
      case '}':
      case '|':
        break;  // We want to exit the for loop but we can only exit the switch.
      default:
        continue;
    }
    break;
  }
  m_stack.pushToken(TOKEN_PLAIN_TEXT, tokenBegin, m_position);
  return true;
}

List CodeParser::constructList(int& index, int& depth, bool stopOnPipe) {
  List list;
  int brokenLinkDepth = 0;
  for (; index < m_stack.size(); index++) {
    const ParserStack::Element& element = m_stack[index];
    if (element.type == NODE_ELEMENT) {
      depth = std::max(depth, element.depth + 1);
      list.addItem(m_stack.extractNodeFromElement(index));
    } else if (stopOnPipe && brokenLinkDepth == 0 && element.type == TOKEN_PIPE) {
      break;
    } else {
      if (element.type != TOKEN_PLAIN_TEXT) {
        switch (element.type) {
          case TOKEN_LINK_BEGIN:
            m_warningsBuffer->add(MISSING_LINK_CLOSURE, element.range.begin, "Unclosed link");
            break;
          case TOKEN_LINK_BROKEN_BEGIN:
            m_warningsBuffer->add(BAD_LINK_OPENING, element.range.begin, "Bad link opening");
            brokenLinkDepth++;
            break;
          case TOKEN_LINK_END:
            if (brokenLinkDepth > 0) {
              brokenLinkDepth--;
            } else {
              m_warningsBuffer->add(MISSING_LINK_OPENING, element.range.begin, "Link closure without opening");
            }
            break;
          case TOKEN_TEMPLATE_BEGIN:
          case TOKEN_TEMPLATE_BEGIN_LEFTOVER: {
            const char* message = "Unclosed template or variable";
            switch (element.range.size()) {
              case 1:
                message = "Extra brace at template or variable opening";
                break;
              case 2:
              case 4:
                message = "Unclosed template";
                break;
              case 3:
                message = "Unclosed variable";
                break;
            }
            m_warningsBuffer->add(MISSING_TEMPLATE_OPENING, element.range.begin, message);
            break;
          }
          case TOKEN_TEMPLATE_END: {
            const char* message = "Template or variable closure without opening";
            switch (element.range.size()) {
              case 1:
                message = "Extra brace at template or variable closure";
                break;
              case 2:
              case 4:
                message = "Template closure without opening";
                break;
              case 3:
                message = "Variable closure without opening";
                break;
            }
            m_warningsBuffer->add(MISSING_TEMPLATE_CLOSURE, element.range.begin, message);
            break;
          }
          default:
            // Nothing to do.
            break;
        }
      }
      if (list.empty() || list[list.size() - 1].type() != NT_TEXT) {
        list.addItem(std::make_unique<Text>());
      }
      list[list.size() - 1].asText().text.append(element.range.begin, element.range.end);
    }
  }
  depth = std::max(depth, list.empty() ? 1 : 2);
  return list;
}

void CodeParser::constructNodeWithFields(int beginIndex, int& depth, NodeWithFields& node) {
  for (int index = beginIndex;; index++) {
    node.addField(constructList(index, depth, true));
    // If there is a pipe and nothing, we still want to create an empty field, so we must test this here rather than in
    // the condition of the loop.
    if (index >= m_stack.size()) break;
  }
}

void CodeParser::reduceLink() {
  int openingIndex = m_stack.getLastLinkOpening();
  if (openingIndex == -1) return;
  const ParserStack::Element& openingElement = m_stack[openingIndex];
  if (openingElement.type == TOKEN_LINK_BROKEN_BEGIN) {
    m_stack.dropLinkBrokenOpening();
    return;
  }
  CBL_ASSERT_EQ(openingElement.type, TOKEN_LINK_BEGIN);
  ParserStack::Element closureElement = m_stack.pop();
  CBL_ASSERT_EQ(closureElement.type, TOKEN_LINK_END);
  unique_ptr<Link> link = std::make_unique<Link>();
  int depth = 0;
  constructNodeWithFields(openingIndex + 1, depth, *link);
  if (m_warningsBuffer->enabledWarnings() & LINK_WITH_LINE_BREAK) {
    for (const Node& node : (*link)[0]) {
      if (node.type() == NT_TEXT && node.asText().text.find('\n') != string::npos) {
        m_warningsBuffer->add(LINK_WITH_LINE_BREAK, openingElement.range.begin,
                              "Link whose target contains a line break");
        break;
      }
    }
  }
  link->computeTarget();
  m_stack.popMany(openingIndex);
  m_stack.pushNode(std::move(link), depth + 1);
}

void CodeParser::reduceTemplateOrVariable() {
  bool canReduce = true;
  while (canReduce) {
    int openingIndex = m_stack.getLastTemplateOpening();
    if (openingIndex == -1) return;
    ParserStack::Element closureElement = m_stack.pop();
    const ParserStack::Element& oldOpeningElement = m_stack[openingIndex];
    CBL_ASSERT(oldOpeningElement.type == TOKEN_TEMPLATE_BEGIN && oldOpeningElement.range.size() >= 2 &&
               closureElement.type == TOKEN_TEMPLATE_END && closureElement.range.size() >= 2);
    ParserStack::Element openingElement(TOKEN_TEMPLATE_BEGIN, oldOpeningElement.range.begin,
                                        oldOpeningElement.range.end);
    unique_ptr<Node> newNode;
    int depth = 0;
    if (openingElement.range.size() >= 3 && closureElement.range.size() >= 3) {
      int indexInVar = openingIndex + 1;
      unique_ptr<Variable> variable = std::make_unique<Variable>(constructList(indexInVar, depth, true));
      if (indexInVar < m_stack.size()) {
        variable->mutableDefaultValue() = constructList(indexInVar + 1, depth);
      }
      openingElement.range.end -= 3;
      closureElement.range.begin += 3;
      newNode = std::move(variable);
    } else {
      TemplatePtr template_ = std::make_unique<Template>();
      constructNodeWithFields(openingIndex + 1, depth, *template_);
      template_->computeName();
      openingElement.range.end -= 2;
      closureElement.range.begin += 2;
      newNode = std::move(template_);
    }
    m_stack.popMany(openingIndex);
    if (!openingElement.range.empty()) {
      if (openingElement.range.size() < 2) {
        // Change the type so that it is not put back in openedElements.
        openingElement.type = TOKEN_TEMPLATE_BEGIN_LEFTOVER;
      }
      m_stack.pushElement(std::move(openingElement));
    }
    m_stack.pushNode(std::move(newNode), depth + 1);
    canReduce = closureElement.range.size() >= 2;
    if (!closureElement.range.empty()) {
      m_stack.pushElement(std::move(closureElement));
    }
  }
}

void CodeParser::reduce() {
  switch (m_stack.back().type) {
    case TOKEN_LINK_END:
      reduceLink();
      break;
    case TOKEN_TEMPLATE_END:
      reduceTemplateOrVariable();
      break;
    default:
      break;
  }
}

void CodeParser::reparseLinksIfNeeded(int beginIndex) {
  if (!(m_stack.getLastTemplateOpening(/* skipLinks = */ true) >= beginIndex &&
        m_stack.getLastLinkOpening(/* skipTemplates = */ true) >= beginIndex)) {
    return;
  }
  vector<ParserStack::Element> reversedEndOfStack;
  reversedEndOfStack.reserve(m_stack.size() - beginIndex);
  while (m_stack.size() > beginIndex) {
    reversedEndOfStack.push_back(m_stack.pop());
  }
  for (; !reversedEndOfStack.empty(); reversedEndOfStack.pop_back()) {
    ParserStack::Element& element = reversedEndOfStack.back();
    if (element.type == TOKEN_TEMPLATE_BEGIN) {
      element.type = TOKEN_TEMPLATE_BEGIN_LEFTOVER;
    }
    m_stack.pushElement(std::move(element));
    if (m_stack.back().type == TOKEN_LINK_END) {
      reduceLink();
    }
  }
}

List CodeParser::parse() {
  const char* codeStart = m_position;
  while (parseToken()) {
    reduce();
  }
  reparseLinksIfNeeded(0);
  if (m_stack.maxDepthReached()) {
    m_warningsBuffer->add(MAX_DEPTH_REACHED, codeStart, "Maximum parser depth reached");
  }
  return constructList(0, m_totalDepth);
}

int getCodeDepth(string_view code) {
  WarningsBuffer warningsBuffer(code.data(), code.data() + code.size(), 0);
  ClosingTagFinder closingTagFinder(code.data(), code.data() + code.size());
  CodeParser parser(code.data(), code.data() + code.size(), &warningsBuffer, &closingTagFinder);
  parser.parse();
  return parser.totalDepth();
}

int setParserMaxDepth(int maxDepth) {
  return ParserStack::setParserMaxDepth(maxDepth);
}

}  // namespace parser_internal

List parse(const char* codeBegin, const char* codeEnd, ErrorLevel level) {
  parser_internal::WarningsBuffer warningsBuffer(codeBegin, codeEnd,
                                                 level == STRICT ? parser_internal::ALL_WARNINGS : 0);
  parser_internal::ClosingTagFinder closingTagFinder(codeBegin, codeEnd);
  parser_internal::CodeParser parser(codeBegin, codeEnd, &warningsBuffer, &closingTagFinder);
  List parsedCode = parser.parse();
  if (level == STRICT && !warningsBuffer.empty()) {
    throw ParseError(warningsBuffer.toString());
  }
  return parsedCode;
}

}  // namespace wikicode
