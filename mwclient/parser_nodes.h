// Parsed representation of wikicode.
// See parser.h for the parse() function that creates this representation.
//
// Supported wikicode elements are comments, tags, internal links, templates and variables.
// For instance, the parsed representation of "[[Example|examples]] are useful" is:
//   List
//     Link
//       List
//         Text("Example")
//       List
//         Text("examples")
//     Text(" are useful")
//
// All elements derive from the base class Node. In the representation created by the parser, the root element is always
// a List and there is an alternance between Lists and other types of nodes as the depth increases (all nodes at depths
// 0, 2, 4, ... are lists, and all nodes at depths 1, 3, 5, ... are not lists).
//
// Helper functions are available to iterate over all nodes of a specific type in the tree. For instance:
//   for (wikicode::Link& link : node.getLinks()) { /* Do something on link. */ }
//
// A node does not store a pointer to its own parent. However, due to the structure of the tree, iterating over all
// direct children of all lists is the same as iterating over all nodes, so it is possible to use the following pattern
// for operations that replace nodes or depend on the context:
//   for (wikicode::List& list : node.getLists()) {
//     for (int i = 0; i < list.size(); i++) {
//       /* Read, modify or delete list[i], possibly using list[i-1] or list[i+1]. */
//     }
//   }
//
// In last resort, for tree traversal with full access to the path between the root and the current node, use
// NodeGenerator.
//
// For the purpose of memory management, a Node is the owner of all its descendants. In particular, removing or
// replacing a node in a List destroys all its descendants, unless the node is moved somewhere else.
//
// After parsing, it is possible to modify nodes and convert the result back to a string.
// In addition to base properties that are used when converting nodes back to strings, some types of nodes have derived
// properties provided for convenience, such as name() for Template. Base properties are always mutable, whereas derived
// properties are sometimes read-only and can get out of sync with base properties when the node changes afer parsing.
//
// Limitations:
// - Magic words and parser functions are represented in the same way as templates.
// - External links are not detected and go to Text nodes.
// - Each tag either contains normal wikicode (e.g. for <ref>) or raw text (e.g. for <nowiki>). There are no per-tag
//   specific structures, e.g. the content of <gallery> tags is not parsed line by line.
#ifndef MWC_PARSER_NODES_H
#define MWC_PARSER_NODES_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/generated_range.h"

namespace wikicode {

namespace parser_internal {
class CodeParser;
}  // namespace parser_internal

constexpr int NO_TYPE_FILTERING = -1;

enum NodeType {
  NT_LIST,      // A list of nodes of other types.
  NT_TEXT,      // "Some text"
  NT_COMMENT,   // "<!-- Some comment -->"
  NT_TAG,       // "<ref>Some reference</ref>"
  NT_LINK,      // "[[Some page]]"
  NT_TEMPLATE,  // "{{Some template}}"
  NT_VARIABLE,  // "{{{Some variable}}}"
};

class Node;
class List;
class Text;
class Comment;
class Tag;
class Link;
class Variable;
class Template;

using NodePtr = std::unique_ptr<Node>;
using LinkPtr = std::unique_ptr<Link>;
using TemplatePtr = std::unique_ptr<Template>;

// Iterator that dereferences pointers, e.g. to iterate over a vector of unique_ptr without exposing the pointers.
template <class Target, class Pointer>
class pointer_it : public std::iterator<std::random_access_iterator_tag, Target> {
public:
  explicit pointer_it(const Pointer* p = nullptr) : p(p) {}
  bool operator==(const pointer_it<Target, Pointer>& it) const { return p == it.p; }
  bool operator!=(const pointer_it<Target, Pointer>& it) const { return p != it.p; }
  Target* operator->() const { return *p; }
  Target& operator*() const { return **p; }
  pointer_it<Target, Pointer> operator++(int) { return pointer_it<Target, Pointer>(p++); }
  pointer_it<Target, Pointer> operator++() { return pointer_it<Target, Pointer>(++p); }
  pointer_it<Target, Pointer> operator--(int) { return pointer_it<Target, Pointer>(p--); }
  pointer_it<Target, Pointer> operator--() { return pointer_it<Target, Pointer>(--p); }
  pointer_it<Target, Pointer> operator+(int i) const { return pointer_it<Target, Pointer>(p + i); }
  pointer_it<Target, Pointer> operator-(int i) const { return pointer_it<Target, Pointer>(p - i); }
  bool operator<(const pointer_it<Target, Pointer>& it) const { return p < it.p; }
  bool operator<=(const pointer_it<Target, Pointer>& it) const { return p <= it.p; }
  bool operator>(const pointer_it<Target, Pointer>& it) const { return p > it.p; }
  bool operator>=(const pointer_it<Target, Pointer>& it) const { return p >= it.p; }
  int operator-(const pointer_it<Target, Pointer>& it) const { return p - it.p; }

private:
  const Pointer* p;
};

enum EnumerationOrder { PREFIX_DFS, POSTFIX_DFS };

// Class for traversing a tree of nodes.
// Can be used through the Node::getNodes or directly to also get access to more context (parent, current depth).
class NodeGenerator {
public:
  using value_type = Node&;
  explicit NodeGenerator(Node* node, EnumerationOrder enumerationOrder = PREFIX_DFS,
                         int typeFiltering = NO_TYPE_FILTERING);
  bool next();
  // Modifying the value is fine independently of enumerationOrder.
  Node& value() const { return *m_stack.back().node; }
  Node* ancestor(int level) const;
  int indexInAncestor(int level) const;
  Node* parent() const { return ancestor(1); }
  // If parent is a List, then value() is parent->asList()[indexInParent]. In the enumeration order is POSTFIX_DFS, this
  // may be used to replace the current node with a node of a different type.
  int indexInParent() const { return indexInAncestor(1); }
  int depth() const { return m_stack.size(); }

private:
  template <class T>
  bool pushChildToStack(T& node, int childIndex);

  struct StackEntry {
    Node* node;
    int childIndex;
  };
  std::vector<StackEntry> m_stack;
  EnumerationOrder m_enumerationOrder = PREFIX_DFS;
  int m_typeFiltering = NO_TYPE_FILTERING;
};

template <class T>
class TypedNodeGenerator {
public:
  using value_type = T&;
  TypedNodeGenerator(Node* node, EnumerationOrder enumerationOrder)
      : m_generator(node, enumerationOrder, T::typeFilter) {}
  TypedNodeGenerator(const Node* node, EnumerationOrder enumerationOrder)
      : m_generator(const_cast<Node*>(node), enumerationOrder, T::typeFilter) {
    static_assert(std::is_const<T>(),
                  "TypedNodeGenerator constructor requires a non-const pointer for iteration on non-const nodes");
  }
  bool next() { return m_generator.next(); }
  T& value() const { return static_cast<T&>(m_generator.value()); }

private:
  NodeGenerator m_generator;
};

class Node {
public:
  explicit Node(NodeType type) : m_type(type) {}
  Node(const Node&) = delete;
  virtual ~Node();
  void operator=(const Node&) = delete;
  // Returns a deep copy of the node.
  virtual NodePtr copyAsNode() const = 0;

  NodeType type() const { return m_type; }

  // Functions to write range-based loops on the descendants of a node, including the node itself if it matches the
  // requested type.
  // WARNING: do not write a for loop that calls this function on a temporary and iterates on the result, such as
  // "for (const Node& node : parse(...).getNodes())", since the root node would be destroyed immediately.
  cbl::GeneratedRange<NodeGenerator> getNodes(EnumerationOrder enumerationOrder = PREFIX_DFS);
  cbl::GeneratedRange<TypedNodeGenerator<const Node>> getNodes(EnumerationOrder enumerationOrder = PREFIX_DFS) const;
  cbl::GeneratedRange<TypedNodeGenerator<List>> getLists(EnumerationOrder enumerationOrder = PREFIX_DFS);
  cbl::GeneratedRange<TypedNodeGenerator<const List>> getLists(EnumerationOrder enumerationOrder = PREFIX_DFS) const;
  cbl::GeneratedRange<TypedNodeGenerator<Tag>> getTags(EnumerationOrder enumerationOrder = PREFIX_DFS);
  cbl::GeneratedRange<TypedNodeGenerator<const Tag>> getTags(EnumerationOrder enumerationOrder = PREFIX_DFS) const;
  cbl::GeneratedRange<TypedNodeGenerator<Link>> getLinks(EnumerationOrder enumerationOrder = PREFIX_DFS);
  cbl::GeneratedRange<TypedNodeGenerator<const Link>> getLinks(EnumerationOrder enumerationOrder = PREFIX_DFS) const;
  cbl::GeneratedRange<TypedNodeGenerator<Template>> getTemplates(EnumerationOrder enumerationOrder = PREFIX_DFS);
  cbl::GeneratedRange<TypedNodeGenerator<const Template>> getTemplates(
      EnumerationOrder enumerationOrder = PREFIX_DFS) const;
  cbl::GeneratedRange<TypedNodeGenerator<Variable>> getVariables(EnumerationOrder enumerationOrder = PREFIX_DFS);
  cbl::GeneratedRange<TypedNodeGenerator<const Variable>> getVariables(
      EnumerationOrder enumerationOrder = PREFIX_DFS) const;

  // Cast to more specific types. NO CHECK PERFORMED.
  inline List& asList();
  inline const List& asList() const;
  inline Text& asText();
  inline const Text& asText() const;
  inline Comment& asComment();
  inline const Comment& asComment() const;
  inline Tag& asTag();
  inline const Tag& asTag() const;
  inline Link& asLink();
  inline const Link& asLink() const;
  inline Template& asTemplate();
  inline const Template& asTemplate() const;
  inline Variable& asVariable();
  inline const Variable& asVariable() const;

  // Appends the string representation of this node to buffer.
  virtual void addToBuffer(std::string& buffer) const = 0;
  // Returns the string representation of this node.
  // For any string, wikicode::parse(code).toString() == code.
  std::string toString() const;

  static constexpr int typeFilter = NO_TYPE_FILTERING;

private:
  const NodeType m_type;
};

// A List is a node grouping several adjacent nodes.
class List : public Node {
public:
  List() : Node(NT_LIST) {}
  List(List&& list) : Node(NT_LIST), m_nodes(std::move(list.m_nodes)) {}
  explicit List(const std::string& s);
  List& operator=(List&& list);
  List copy() const;
  NodePtr copyAsNode() const override;
  void addToBuffer(std::string& buffer) const override;

  using iterator = pointer_it<Node, std::unique_ptr<Node>>;
  iterator begin() const { return iterator(m_nodes.empty() ? nullptr : &m_nodes[0]); }
  iterator end() const { return begin() + m_nodes.size(); }

  Node& operator[](int i) { return *m_nodes[i]; }
  const Node& operator[](int i) const { return *m_nodes[i]; }
  int size() const { return m_nodes.size(); }
  bool empty() const { return m_nodes.empty(); }
  void resize(int size) { m_nodes.resize(size); }
  void clear() { m_nodes.clear(); }

  // Replaces a specific item in the list.
  // Returns the previous item, in case you want to avoid its recursive destruction.
  NodePtr setItem(int i, NodePtr item);
  // Convenience wrapper that puts a Text node with text `content` in the list.
  NodePtr setItem(int i, const std::string& content);
  // Adds an item in the specified position (0 <= i <= size()).
  // Complexity: linear in the size of the list.
  void addItem(int i, NodePtr item);
  void addItem(int i, const std::string& content);
  // Adds an item at the end of the list.
  // Amortized complexity: constant time.
  void addItem(NodePtr item) { addItem(size(), std::move(item)); }
  void addItem(const std::string& content) { addItem(size(), content); }
  // Removes the item at the specified position, shifting all the items after it.
  // Complexity: linear in the size of the list.
  NodePtr removeItem(int i);

  static constexpr int typeFilter = NT_LIST;

private:
  std::vector<NodePtr> m_nodes;
};

// Base class for Link and Template.
class NodeWithFields : public Node {
public:
  using Node::Node;

  List& operator[](int i) { return m_fields[i]; }
  const List& operator[](int i) const { return m_fields[i]; }
  int size() const { return m_fields.size(); }
  bool empty() const { return m_fields.empty(); }

  // Modification of fields.
  List setField(int i, List&& item);
  List setField(int i, const std::string& content) { return setField(i, List(content)); }
  void addField(int i, List&& item);
  void addField(int i, const std::string& content) { addField(i, List(content)); }
  void addField(List&& item) { addField(size(), std::move(item)); }
  void addField(const std::string& content) { addField(size(), content); }
  List removeField(int i);
  void removeAllFieldsExceptFirst() { m_fields.resize(1); }

protected:
  std::vector<List> m_fields;
};

// A Text contains an arbitrary string without any special wikicode element interpreted by the parser.
// There are however cases where wikicode elements may end up in a Text element, for instance if the maximum parsing
// depth is exceeded.
class Text : public Node {
public:
  Text() : Node(NT_TEXT) {}
  explicit Text(std::string_view s) : Node(NT_TEXT), text(s) {}
  NodePtr copyAsNode() const override;
  void addToBuffer(std::string& buffer) const override;

  std::string text;

  static constexpr int typeFilter = NT_TEXT;
};

// A Comment is a piece of code that starts with "<!--" and usually ends with "-->".
// Special case: a "<!--" without matching "-->" is interpreted as a comment that goes until the end of the page.
class Comment : public Node {
public:
  Comment() : Node(NT_COMMENT) {}
  NodePtr copyAsNode() const override;
  void addToBuffer(std::string& buffer) const override;

  // In the output of the parser, starts with "<!--" and typically ends with "-->".
  std::string text;

  static constexpr int typeFilter = NT_COMMENT;
};

// A Tag corresponds to a MediaWiki parser extension tag and its content, e.g. "<ref>Some book</ref>".
// Tags may be self-closing, e.g. "<ref name="x" />". In that case, there is no content and no closing tag.
// HTML tags like "<b>" are not parsed and go to text elements. Inclusion tags (<includeonly>, <noinclude> and
// <onlyinclude>) are not parsed. A dedicated parser is available in mwclient/util/include_tags.h.
class Tag : public Node {
public:
  Tag() : Node(NT_TAG) {}
  NodePtr copyAsNode() const override;
  void addToBuffer(std::string& buffer) const override;

  // Lower case name of the tag, e.g. "ref" for "<Ref name='abc'>".
  // Derived from openingTag() during parsing. Not automatically updated later. Not used for conversion to text.
  const std::string& tagName() const { return m_tagName; }
  void setTagName(std::string_view value) { m_tagName = value; }

  // Full opening tag, e.g. "<ref name='abc'>".
  const std::string& openingTag() const { return m_openingTag; }
  void setOpeningTag(std::string_view value) { m_openingTag = value; }
  // Full closing tag, e.g. "</ref>".
  // Empty if the tag is self-closing and has no content. May also be empty even if content is non-null. For instance,
  // <pre> tags do not require a closing tag.
  const std::string& closingTag() const { return m_closingTag; }
  void setClosingTag(std::string_view value) { m_closingTag = value; }
  // Content between the opening tag and the closing tag.
  const std::optional<List>& content() const { return m_content; }
  std::optional<List>& mutableContent() { return m_content; }

  static constexpr int typeFilter = NT_TAG;

private:
  std::string m_tagName;
  std::string m_openingTag;
  std::string m_closingTag;
  std::optional<List> m_content;
};

// A Link is a wikicode element written the syntax [[...]].
// Apart from normal links like [[Wikipedia]], this includes category links, file links and interwiki links.
// The parser of this library can produce arbitrarily nested links, although MediaWiki only allows this in files
// (e.g. "[[File:A.jpg|thumb|This is an [[image]].]]").
// All pipes are field separators. For instance, [[<target>|Y|Z]] is parsed as a link with three fields, although for
// MediaWiki, this is just a link with title "Y|Z" unless <target> is a file.
class Link : public NodeWithFields {
public:
  Link() : NodeWithFields(NT_LINK) {}
  LinkPtr copy() const;
  NodePtr copyAsNode() const override;
  void addToBuffer(std::string& buffer) const override;

  // The following fields are derived from m_fields[0] during parsing and cannot be updated later.

  // Prenormalized target without the anchor (like Wiki::normalizeTitle, but preserves the leading ":", does not
  // normalize namespaces and does not put the first letter in upper case).
  const std::string& target() const { return m_target; }
  // Normalized anchor. Either empty or starts with "#".
  const std::string& anchor() const { return m_anchor; }
  // Whether the link has an initial ":" to remove the specific interpretation of interlang links, category links and
  // file links.
  // TODO: For the corner case [[_:page]] where target() == ":page", this should return false.
  bool targetStartsWithColon() const;

  static constexpr int typeFilter = NT_LINK;

private:
  void computeTarget();

  std::string m_target;
  std::string m_anchor;

  friend class parser_internal::CodeParser;
};

constexpr const char* UNNAMED_PARAM = "=0";  // Arbitrary string that cannot be a valid parameter name in a template.

enum SplitOptions {
  // Parameter normalization
  NORMALIZE_PARAM = 1,  // Strip comments, trim leading and trailing spaces, and merge consecutive spaces.

  // Value normalization.
  TRIM_VALUE = 2,
  TRIM_AND_COLLAPSE_SPACE_IN_VALUE = 4,
  STRIP_COMMENTS_IN_VALUE = 8,
  NORMALIZE_VALUE = TRIM_VALUE | STRIP_COMMENTS_IN_VALUE,
  NORMALIZE_COLLAPSE_VALUE = TRIM_AND_COLLAPSE_SPACE_IN_VALUE | STRIP_COMMENTS_IN_VALUE,
};

struct TemplateField {
  std::string param;
  std::string value;
  int index = 0;
};

constexpr int FIND_PARAM_NONE = -1;

class ParsedFields {
public:
  using FieldsMap = std::unordered_map<std::string_view, TemplateField*>;

  struct iterator {
    bool operator!=(const iterator& it) const { return mapIt != it.mapIt; }
    const TemplateField& operator*() const { return *mapIt->second; }
    iterator operator++() { return {++mapIt}; }
    FieldsMap::const_iterator mapIt;
  };

  explicit ParsedFields(std::vector<TemplateField>&& orderedFields);
  ParsedFields(const ParsedFields&) = delete;  // Keys of m_fieldsMap would not be correct.
  ParsedFields(ParsedFields&&) = default;
  ParsedFields& operator=(const ParsedFields&) = delete;  // Keys of m_fieldsMap would not be correct.
  ParsedFields& operator=(ParsedFields&&) = default;

  // Returns the value of parameter `param` or an empty string if it is not defined.
  // Example: for {{t|param=value|value2}}, parsedFields["param"] is "value" and parsedFields["1"] is "value2".
  const std::string& operator[](std::string_view param) const;
  // Returns the value of parameter `param` or `defaultValue` if it is not defined.
  std::string getWithDefault(std::string_view param, std::string_view defaultValue) const;
  // Returns the index of parameter `param` in the fields of the template.
  // If there is no such parameter, returns FIND_PARAM_NONE.
  // Example: for {{t|param=value|value2}}, indexOf("param") is 1 and indexOf("1") is 2.
  int indexOf(std::string_view param) const;
  // Returns true if the parameter `param` is set in the template, false otherwise.
  // Example: for {{t|x1=value|x2=}}, contains("x1") is true, contains("x2") is true and contains("x3") is false.
  bool contains(std::string_view param) const;

  // Iteration on fields in unspecified order. For duplicate fields, only the last occurrence is returned.
  iterator begin() const { return {m_fieldsMap.begin()}; }
  iterator end() const { return {m_fieldsMap.end()}; }
  // Iteration on fields from the first to the last. For duplicate fields, all occurrences are returned.
  const std::vector<TemplateField> orderedFields() const { return m_orderedFields; }

private:
  std::vector<TemplateField> m_orderedFields;
  FieldsMap m_fieldsMap;  // Keys are string_views pointing to strings in m_fields.
};

// A Template is a wikicode element written the syntax {{...}}.
// In the current implementation, there is no specific behavior for parser functions like {{#if:...}}, so they are
// parsed as templates.
class Template : public NodeWithFields {
public:
  Template() : NodeWithFields(NT_TEMPLATE) {}
  explicit Template(const std::string& name);
  TemplatePtr copy() const;
  NodePtr copyAsNode() const override;

  void addToBuffer(std::string& buffer) const override;

  // Prenormalized name of the template (like Wiki::normalizeTitle, but preserves the leading ":", does not normalize
  // namespaces and does not put the first letter in upper case).
  // If the name field contains anything else than text and comments, name() returns an empty string.
  // In general, anything after '#' is removed ({{mytemplate#anchor}} => "mytemplate"). There is currently an exception
  // for parser functions, but do not rely on it. If you need to detect parser functions, read field 0 directly.
  // The value is derived from m_fields[0] during parsing and cannot be updated later.
  const std::string& name() const { return m_name; }

  // Computes a read-only param => value map from all fields.
  ParsedFields getParsedFields(int valueOptions = NORMALIZE_VALUE) const;
  // Changes the parameter name of field i without changing the value.
  // Existing space before and after the name is preserved. However, this may not be optimal in multiline templates
  // where equal signs are aligned.
  List setFieldName(int i, const std::string& name);
  // Changes the value of field i without changing the parameter name.
  // If there is already a value, space before and after the value are be preserved. However, if a value is set for a
  // previously empty parameter, the formatting may not be consistent with other fields.
  List setFieldValue(int i, const std::string& value);

  // Splits the parameter (part before '=') and the value (part after '=') in field `fieldIndex`.
  // If the field does not contain '=', *param is set to UNNAMED_PARAM and *value is set to the entire content.
  // Note that the empty string is a valid parameter name, i.e. a field containing "=Some value" will parsed as
  // (param = "", value = "Some value").
  // options should be a combination of values from SplitOptions.
  void splitParamValue(int fieldIndex, std::string* param, std::string* value, int options = NORMALIZE_PARAM) const;

  static constexpr int typeFilter = NT_TEMPLATE;

private:
  void computeName();

  std::string m_name;

  friend class parser_internal::CodeParser;
};

// A Variable is a wikicode element written the syntax {{{...}}}.
class Variable : public Node {
public:
  explicit Variable(List&& nameNode) : Node(NT_VARIABLE), m_nameNode(std::move(nameNode)) {}
  NodePtr copyAsNode() const override;
  void addToBuffer(std::string& buffer) const override;
  List& nameNode() { return m_nameNode; }
  const List& nameNode() const { return m_nameNode; }
  void setNameNode(List&& node) { m_nameNode = std::move(node); }
  const std::optional<List>& defaultValue() const { return m_defaultValue; }
  std::optional<List>& mutableDefaultValue() { return m_defaultValue; }

  static constexpr int typeFilter = NT_VARIABLE;

private:
  List m_nameNode;
  std::optional<List> m_defaultValue;
};

inline List& Node::asList() {
  return static_cast<List&>(*this);
}
inline const List& Node::asList() const {
  return static_cast<const List&>(*this);
}
inline Text& Node::asText() {
  return static_cast<Text&>(*this);
}
inline const Text& Node::asText() const {
  return static_cast<const Text&>(*this);
}
inline Comment& Node::asComment() {
  return static_cast<Comment&>(*this);
}
inline const Comment& Node::asComment() const {
  return static_cast<const Comment&>(*this);
}
inline Tag& Node::asTag() {
  return static_cast<Tag&>(*this);
}
inline const Tag& Node::asTag() const {
  return static_cast<const Tag&>(*this);
}
inline Link& Node::asLink() {
  return static_cast<Link&>(*this);
}
inline const Link& Node::asLink() const {
  return static_cast<const Link&>(*this);
}
inline Template& Node::asTemplate() {
  return static_cast<Template&>(*this);
}
inline const Template& Node::asTemplate() const {
  return static_cast<const Template&>(*this);
}
inline Variable& Node::asVariable() {
  return static_cast<Variable&>(*this);
}
inline const Variable& Node::asVariable() const {
  return static_cast<const Variable&>(*this);
}

}  // namespace wikicode

#endif
