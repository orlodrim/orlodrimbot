#include "parser_nodes.h"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "cbl/generated_range.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "parser_misc.h"
#include "site_info.h"
#include "titles_util.h"

using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace wikicode {

static bool extractDummyVariableText(const Variable& variable, string& rawText) {
  if (!variable.defaultValue().has_value()) {
    return false;
  }
  for (const Node& node : variable.nameNode()) {
    switch (node.type()) {
      case NT_TEXT:
        if (!cbl::isSpace(node.asText().text)) {
          return false;
        }
        break;
      case NT_COMMENT:
        break;
      default:
        return false;
    }
  }
  for (const Node& node : *variable.defaultValue()) {
    switch (node.type()) {
      case NT_TEXT:
        rawText += node.asText().text;
        break;
      case NT_COMMENT:
        break;
      default:
        return false;
    }
  }
  return true;
}

static string_view stripSubst(string_view name) {
  constexpr string_view SUBST_PREFIX = "subst:";
  constexpr string_view SAFESUBST_PREFIX = "safesubst:";
  string_view strippedName = cbl::trim(name, cbl::TRIM_LEFT);
  if (strippedName.starts_with(SUBST_PREFIX)) {
    return strippedName.substr(SUBST_PREFIX.size());
  } else if (strippedName.starts_with(SAFESUBST_PREFIX)) {
    return strippedName.substr(SAFESUBST_PREFIX.size());
  }
  return name;
}

/* == NodeGenerator == */

NodeGenerator::NodeGenerator(Node* node, EnumerationOrder enumerationOrder, int typeFiltering)
    : m_stack({{node, -2}}), m_enumerationOrder(enumerationOrder), m_typeFiltering(typeFiltering) {}

Node* NodeGenerator::ancestor(int level) const {
  int index = static_cast<int>(m_stack.size()) - 1 - level;
  return index >= 0 ? m_stack[index].node : nullptr;
}

int NodeGenerator::indexInAncestor(int level) const {
  int index = static_cast<int>(m_stack.size()) - 1 - level;
  return index >= 0 ? m_stack[index].childIndex : 0;
}

template <class T>
bool NodeGenerator::pushChildToStack(T& node, int childIndex) {
  if (childIndex < node.size()) {
    m_stack.push_back({&node[childIndex], -1});
    return true;
  }
  return false;
}

bool NodeGenerator::next() {
  if (m_enumerationOrder == POSTFIX_DFS && !m_stack.empty() && m_stack.back().childIndex != -2) {
    m_stack.pop_back();
  }
  bool returnOnPush = m_enumerationOrder == PREFIX_DFS;
  while (!m_stack.empty()) {
    Node* currentNode = m_stack.back().node;
    int& childIndex = m_stack.back().childIndex;
    childIndex++;
    bool nodePushed = false;
    if (childIndex == -1) {
      // This only happens on the first iteration, when visiting the root.
      nodePushed = true;
    } else {
      switch (currentNode->type()) {
        case NT_LIST:
          nodePushed = pushChildToStack(currentNode->asList(), childIndex);
          break;
        case NT_TEXT:
        case NT_COMMENT:
          break;
        case NT_TAG: {
          Tag& tag = currentNode->asTag();
          if (childIndex == 0 && tag.content()) {
            m_stack.push_back({&*tag.mutableContent(), -1});
            nodePushed = true;
          }
          break;
        }
        case NT_LINK:
          nodePushed = pushChildToStack(currentNode->asLink(), childIndex);
          break;
        case NT_TEMPLATE:
          nodePushed = pushChildToStack(currentNode->asTemplate(), childIndex);
          break;
        case NT_VARIABLE: {
          Variable& variable = currentNode->asVariable();
          if (childIndex == 0) {
            m_stack.push_back({&variable.nameNode(), -1});
            nodePushed = true;
          } else if (childIndex == 1 && variable.defaultValue()) {
            m_stack.push_back({&*variable.mutableDefaultValue(), -1});
            nodePushed = true;
          }
          break;
        }
      }
    }
    if (nodePushed == returnOnPush &&
        (m_typeFiltering == NO_TYPE_FILTERING || m_stack.back().node->type() == m_typeFiltering)) {
      return true;
    } else if (!nodePushed) {
      m_stack.pop_back();
    }
  }
  return false;
}

/* == Node == */

Node::~Node() {}

cbl::GeneratedRange<NodeGenerator> Node::getNodes(EnumerationOrder enumerationOrder) {
  return cbl::GeneratedRange<NodeGenerator>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<const Node>> Node::getNodes(EnumerationOrder enumerationOrder) const {
  return cbl::GeneratedRange<TypedNodeGenerator<const Node>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<List>> Node::getLists(EnumerationOrder enumerationOrder) {
  return cbl::GeneratedRange<TypedNodeGenerator<List>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<const List>> Node::getLists(EnumerationOrder enumerationOrder) const {
  return cbl::GeneratedRange<TypedNodeGenerator<const List>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<Tag>> Node::getTags(EnumerationOrder enumerationOrder) {
  return cbl::GeneratedRange<TypedNodeGenerator<Tag>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<const Tag>> Node::getTags(EnumerationOrder enumerationOrder) const {
  return cbl::GeneratedRange<TypedNodeGenerator<const Tag>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<Link>> Node::getLinks(EnumerationOrder enumerationOrder) {
  return cbl::GeneratedRange<TypedNodeGenerator<Link>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<const Link>> Node::getLinks(EnumerationOrder enumerationOrder) const {
  return cbl::GeneratedRange<TypedNodeGenerator<const Link>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<Template>> Node::getTemplates(EnumerationOrder enumerationOrder) {
  return cbl::GeneratedRange<TypedNodeGenerator<Template>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<const Template>> Node::getTemplates(EnumerationOrder enumerationOrder) const {
  return cbl::GeneratedRange<TypedNodeGenerator<const Template>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<Variable>> Node::getVariables(EnumerationOrder enumerationOrder) {
  return cbl::GeneratedRange<TypedNodeGenerator<Variable>>(this, enumerationOrder);
}

cbl::GeneratedRange<TypedNodeGenerator<const Variable>> Node::getVariables(EnumerationOrder enumerationOrder) const {
  return cbl::GeneratedRange<TypedNodeGenerator<const Variable>>(this, enumerationOrder);
}

string Node::toString() const {
  string buffer;
  addToBuffer(buffer);
  return buffer;
}

/* == List == */

List& List::operator=(List&& list) {
  m_nodes = std::move(list.m_nodes);
  return *this;
}

List::List(const string& s) : Node(NT_LIST) {
  if (!s.empty()) {
    m_nodes.push_back(std::make_unique<Text>(s));
  }
}

List List::copy() const {
  int size = m_nodes.size();
  List newList;
  newList.m_nodes.resize(size);
  for (int i = 0; i < size; i++) {
    newList.m_nodes[i] = m_nodes[i]->copyAsNode();
  }
  return newList;
}

unique_ptr<Node> List::copyAsNode() const {
  return std::make_unique<List>(copy());
}

void List::addToBuffer(std::string& buffer) const {
  for (const NodePtr& node : m_nodes) {
    node->addToBuffer(buffer);
  }
}

NodePtr List::setItem(int i, NodePtr item) {
  CBL_ASSERT(i >= 0 && i < size());
  NodePtr oldNode = std::move(m_nodes[i]);
  m_nodes[i] = std::move(item);
  return oldNode;
}

NodePtr List::setItem(int i, const std::string& content) {
  return setItem(i, std::make_unique<Text>(content));
}

void List::addItem(int i, NodePtr item) {
  CBL_ASSERT(i >= 0 && i <= size());
  m_nodes.insert(m_nodes.begin() + i, std::move(item));
}

void List::addItem(int i, const std::string& content) {
  addItem(i, std::make_unique<Text>(content));
}

NodePtr List::removeItem(int i) {
  CBL_ASSERT(i >= 0 && i < size());
  NodePtr oldNode = std::move(m_nodes[i]);
  m_nodes.erase(m_nodes.begin() + i);
  return oldNode;
}

/* == NodeWithFields == */

List NodeWithFields::setField(int i, List&& item) {
  CBL_ASSERT(i >= 0 && i < size());
  List oldField = std::move(m_fields[i]);
  m_fields[i] = std::move(item);
  return oldField;
}

void NodeWithFields::addField(int i, List&& item) {
  CBL_ASSERT(i >= 0 && i <= size());
  m_fields.insert(m_fields.begin() + i, std::move(item));
}

List NodeWithFields::removeField(int i) {
  CBL_ASSERT(i >= 0 && i < size());
  List oldField = std::move(m_fields[i]);
  m_fields.erase(m_fields.begin() + i);
  return oldField;
}

/* == Text == */

unique_ptr<Node> Text::copyAsNode() const {
  return std::make_unique<Text>(text);
}

void Text::addToBuffer(std::string& buffer) const {
  buffer += text;
}

/* == Comment == */

unique_ptr<Node> Comment::copyAsNode() const {
  unique_ptr<Comment> comment = std::make_unique<Comment>();
  comment->text = text;
  return comment;
}

void Comment::addToBuffer(std::string& buffer) const {
  buffer += text;
}

/* == Tag == */

unique_ptr<Node> Tag::copyAsNode() const {
  unique_ptr<Tag> tag = std::make_unique<Tag>();
  tag->m_tagName = m_tagName;
  tag->m_openingTag = m_openingTag;
  tag->m_closingTag = m_closingTag;
  if (m_content) {
    tag->m_content = m_content->copy();
  }
  return std::move(tag);
}

void Tag::addToBuffer(std::string& buffer) const {
  buffer += m_openingTag;
  if (m_content) {
    m_content->addToBuffer(buffer);
  }
  buffer += m_closingTag;
}

/* == Link == */

bool Link::targetStartsWithColon() const {
  return m_target.starts_with(":");
}

LinkPtr Link::copy() const {
  LinkPtr linkCopy = std::make_unique<Link>();
  linkCopy->m_fields.reserve(m_fields.size());
  for (const List& list : m_fields) {
    linkCopy->m_fields.push_back(list.copy());
  }
  linkCopy->m_target = m_target;
  linkCopy->m_anchor = m_anchor;
  return linkCopy;
}

unique_ptr<Node> Link::copyAsNode() const {
  return copy();
}

void Link::addToBuffer(string& buffer) const {
  buffer += "[[";
  for (size_t i = 0; i < m_fields.size(); i++) {
    if (i > 0) {
      buffer += '|';
    }
    m_fields[i].addToBuffer(buffer);
  }
  buffer += "]]";
}

void Link::computeTarget() {
  m_target.clear();
  m_anchor.clear();

  string rawText;
  CBL_ASSERT(!m_fields.empty());
  for (Node& node : m_fields[0]) {
    if (node.type() == NT_TEXT) {
      rawText += node.asText().text;
    } else if (node.type() != NT_COMMENT) {
      return;
    }
  }

  mwc::TitleParts titleParts =
      mwc::TitlesUtil(mwc::SiteInfo::stubInstance())
          .parseTitle(rawText, mwc::NS_MAIN, mwc::PTF_LINK_TARGET | mwc::PTF_KEEP_INITIAL_COLON);
  if (titleParts.titleWithoutAnchor().find('|') == string::npos) {
    m_anchor = string(titleParts.anchor());
    titleParts.clearAnchor();
    m_target = std::move(titleParts.title);
  }
}

/* == Template == */

ParsedFields::ParsedFields(vector<TemplateField>&& orderedFields) : m_orderedFields(std::move(orderedFields)) {
  for (TemplateField& field : m_orderedFields) {
    m_fieldsMap[field.param] = &field;
  }
}

const string& ParsedFields::operator[](string_view param) const {
  static string EMPTY_STRING;
  FieldsMap::const_iterator it = m_fieldsMap.find(param);
  return it != m_fieldsMap.end() ? it->second->value : EMPTY_STRING;
}

string ParsedFields::getWithDefault(string_view param, string_view defaultValue) const {
  FieldsMap::const_iterator it = m_fieldsMap.find(param);
  return it != m_fieldsMap.end() ? it->second->value : string(defaultValue);
}

int ParsedFields::indexOf(string_view param) const {
  FieldsMap::const_iterator it = m_fieldsMap.find(param);
  return it != m_fieldsMap.end() ? it->second->index : FIND_PARAM_NONE;
}

bool ParsedFields::contains(string_view param) const {
  return m_fieldsMap.find(param) != m_fieldsMap.end();
}

Template::Template(const string& name) : NodeWithFields(NT_TEMPLATE) {
  m_fields.push_back(List(name));
  computeName();
}

TemplatePtr Template::copy() const {
  unique_ptr<Template> templateCopy = std::make_unique<Template>();
  templateCopy->m_fields.reserve(m_fields.size());
  for (const List& list : m_fields) {
    templateCopy->m_fields.push_back(list.copy());
  }
  templateCopy->m_name = m_name;
  return templateCopy;
}

unique_ptr<Node> Template::copyAsNode() const {
  return copy();
}

void Template::addToBuffer(string& buffer) const {
  buffer += "{{";
  for (size_t i = 0; i < m_fields.size(); i++) {
    if (i > 0) {
      buffer += '|';
    }
    m_fields[i].addToBuffer(buffer);
  }
  buffer += "}}";
}

ParsedFields Template::getParsedFields(int valueOptions) const {
  int size = m_fields.empty() ? 0 : m_fields.size() - 1;
  vector<TemplateField> orderedFields(size);
  int unnamedParameterIndex = 0;
  for (int i = 0; i < size; i++) {
    TemplateField& field = orderedFields[i];
    field.index = i + 1;
    splitParamValue(field.index, &field.param, &field.value, NORMALIZE_PARAM | valueOptions);
    if (field.param == UNNAMED_PARAM) {
      unnamedParameterIndex++;
      field.param = std::to_string(unnamedParameterIndex);
    }
  }
  return ParsedFields(std::move(orderedFields));
}

List Template::setFieldName(int i, const string& name) {
  string oldParam, value;
  splitParamValue(i, &oldParam, &value, 0);

  if (oldParam == UNNAMED_PARAM) {
    oldParam.clear();
  }
  cbl::StringBorders borders = cbl::getTrimmedBorders(oldParam, cbl::TRIM_BOTH);
  if (borders.left == borders.right) {
    borders.left = 0;
    borders.right = 0;
  }
  string newText = oldParam.substr(0, borders.left) + name + oldParam.substr(borders.right) + "=" + value;
  return setField(i, newText);
}

List Template::setFieldValue(int i, const string& value) {
  string param, oldValue;
  splitParamValue(i, &param, &oldValue, 0);

  cbl::StringBorders borders = cbl::getTrimmedBorders(oldValue, cbl::TRIM_BOTH);
  if (borders.left == borders.right) {
    borders.left = !oldValue.empty() && oldValue[0] == ' ' ? 1 : 0;
    borders.right = borders.left;
  }
  int64_t rightBorderToEnd = oldValue.size() - borders.right;

  string newText;
  newText.reserve(param.size() + 1 + borders.left + value.size() + rightBorderToEnd);
  if (param != UNNAMED_PARAM) {
    newText += param;
    newText += '=';
  }
  newText.append(oldValue, 0, borders.left);
  newText += value;
  newText.append(oldValue, borders.right, rightBorderToEnd);
  return setField(i, newText);
}

void Template::splitParamValue(int fieldIndex, string* param, string* value, int options) const {
  string buffer;
  bool beforeEqual = true;
  bool paramSet = false;
  for (const Node& node : m_fields[fieldIndex]) {
    if (beforeEqual && node.type() == NT_TEXT) {
      const string& text = node.asText().text;
      size_t equalPosition = text.find('=');
      if (equalPosition != string::npos) {
        beforeEqual = false;
        if (equalPosition > 0 && text[equalPosition - 1] == '\n' && equalPosition < text.size() - 1 &&
            text[equalPosition + 1] == '=') {
          // Equal sign in title. This does not separate a parameter from a value.
        } else {
          if (param) {
            buffer.append(text, 0, equalPosition);
            *param = std::move(buffer);
            paramSet = true;
            if (!value) break;
          }
          buffer.assign(text, equalPosition + 1, text.size() - equalPosition - 1);
          continue;
        }
      }
    }
    node.addToBuffer(buffer);
  }
  if (param) {
    if (!paramSet) {
      *param = UNNAMED_PARAM;
    } else if (options & NORMALIZE_PARAM) {
      stripCommentsInPlace(*param);
      *param = cbl::trimAndCollapseSpace(*param);
    }
  }
  if (value) {
    *value = std::move(buffer);
    if (options & STRIP_COMMENTS_IN_VALUE) {
      stripCommentsInPlace(*value);
    }
    if (options & TRIM_AND_COLLAPSE_SPACE_IN_VALUE) {
      *value = cbl::trimAndCollapseSpace(*value);
    } else if (options & TRIM_VALUE) {
      *value = string(cbl::trim(*value));
    }
  }
}

void Template::computeName() {
  CBL_ASSERT(!m_fields.empty());
  List& firstField = m_fields[0];

  m_name.clear();

  string rawText;
  for (const Node& node : firstField) {
    switch (node.type()) {
      case NT_TEXT:
        rawText += node.asText().text;
        break;
      case NT_VARIABLE: {
        if (!extractDummyVariableText(node.asVariable(), rawText)) {
          return;
        }
        break;
      }
      case NT_COMMENT:
        break;
      default:
        return;
    }
  }

  mwc::TitleParts titleParts = mwc::TitlesUtil(mwc::SiteInfo::stubInstance())
                                   .parseTitle(stripSubst(rawText), mwc::NS_MAIN, mwc::PTF_KEEP_INITIAL_COLON);
  if (titleParts.titleWithoutAnchor().empty()) {
    m_name = titleParts.anchor();  // Parser function.
  } else {
    titleParts.clearAnchor();
    m_name = std::move(titleParts.title);
  }
}

/* == Variable == */

unique_ptr<Node> Variable::copyAsNode() const {
  unique_ptr<Variable> varCopy = std::make_unique<Variable>(m_nameNode.copy());
  if (m_defaultValue) {
    varCopy->m_defaultValue = m_defaultValue->copy();
  }
  return varCopy;
}

void Variable::addToBuffer(std::string& buffer) const {
  buffer += "{{{";
  m_nameNode.addToBuffer(buffer);
  if (m_defaultValue) {
    buffer += '|';
    m_defaultValue->addToBuffer(buffer);
  }
  buffer += "}}}";
}

}  // namespace wikicode
