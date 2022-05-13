/*
  ___        _          Version 3.3
 |_ _|_ __  (_) __ _    https://github.com/pantor/inja
  | || '_ \ | |/ _` |   Licensed under the MIT License <http://opensource.org/licenses/MIT>.
  | || | | || | (_| |
 |___|_| |_|/ |\__,_|   Copyright (c) 2018-2021 Lars Berscheid
          |__/
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef INCLUDE_INJA_INJA_HPP_
#define INCLUDE_INJA_INJA_HPP_

#include <nlohmann/json.hpp>

namespace inja {
#ifndef INJA_DATA_TYPE
using json = nlohmann::json;
#else
using json = INJA_DATA_TYPE;
#endif
} // namespace inja

#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)) && !defined(INJA_NOEXCEPTION)
#ifndef INJA_THROW
#define INJA_THROW(exception) throw exception
#endif
#else
#include <cstdlib>
#ifndef INJA_THROW
#define INJA_THROW(exception)                                                                                                                                  \
  std::abort();                                                                                                                                                \
  std::ignore = exception
#endif
#ifndef INJA_NOEXCEPTION
#define INJA_NOEXCEPTION
#endif
#endif

// #include "environment.hpp"
#ifndef INCLUDE_INJA_ENVIRONMENT_HPP_
#define INCLUDE_INJA_ENVIRONMENT_HPP_

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

// #include "config.hpp"
#ifndef INCLUDE_INJA_CONFIG_HPP_
#define INCLUDE_INJA_CONFIG_HPP_

#include <functional>
#include <string>

// #include "template.hpp"
#ifndef INCLUDE_INJA_TEMPLATE_HPP_
#define INCLUDE_INJA_TEMPLATE_HPP_

#include <map>
#include <memory>
#include <string>
#include <vector>

// #include "node.hpp"
#ifndef INCLUDE_INJA_NODE_HPP_
#define INCLUDE_INJA_NODE_HPP_

#include <string>
#include <string_view>
#include <utility>

// #include "function_storage.hpp"
#ifndef INCLUDE_INJA_FUNCTION_STORAGE_HPP_
#define INCLUDE_INJA_FUNCTION_STORAGE_HPP_

#include <string_view>
#include <vector>

namespace inja {

using Arguments = std::vector<const json*>;
using CallbackFunction = std::function<json(Arguments& args)>;
using VoidCallbackFunction = std::function<void(Arguments& args)>;

/*!
 * \brief Class for builtin functions and user-defined callbacks.
 */
class FunctionStorage {
public:
  enum class Operation {
    Not,
    And,
    Or,
    In,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Add,
    Subtract,
    Multiplication,
    Division,
    Power,
    Modulo,
    AtId,
    At,
    Default,
    DivisibleBy,
    Even,
    Exists,
    ExistsInObject,
    First,
    Float,
    Int,
    IsArray,
    IsBoolean,
    IsFloat,
    IsInteger,
    IsNumber,
    IsObject,
    IsString,
    Last,
    Length,
    Lower,
    Max,
    Min,
    Odd,
    Range,
    Round,
    Sort,
    Upper,
    Super,
    Join,
    Callback,
    ParenLeft,
    ParenRight,
    None,
  };

  struct FunctionData {
    explicit FunctionData(const Operation& op, const CallbackFunction& cb = CallbackFunction {}): operation(op), callback(cb) {}
    const Operation operation;
    const CallbackFunction callback;
  };

private:
  const int VARIADIC {-1};

  std::map<std::pair<std::string, int>, FunctionData> function_storage = {
      {std::make_pair("at", 2), FunctionData {Operation::At}},
      {std::make_pair("default", 2), FunctionData {Operation::Default}},
      {std::make_pair("divisibleBy", 2), FunctionData {Operation::DivisibleBy}},
      {std::make_pair("even", 1), FunctionData {Operation::Even}},
      {std::make_pair("exists", 1), FunctionData {Operation::Exists}},
      {std::make_pair("existsIn", 2), FunctionData {Operation::ExistsInObject}},
      {std::make_pair("first", 1), FunctionData {Operation::First}},
      {std::make_pair("float", 1), FunctionData {Operation::Float}},
      {std::make_pair("int", 1), FunctionData {Operation::Int}},
      {std::make_pair("isArray", 1), FunctionData {Operation::IsArray}},
      {std::make_pair("isBoolean", 1), FunctionData {Operation::IsBoolean}},
      {std::make_pair("isFloat", 1), FunctionData {Operation::IsFloat}},
      {std::make_pair("isInteger", 1), FunctionData {Operation::IsInteger}},
      {std::make_pair("isNumber", 1), FunctionData {Operation::IsNumber}},
      {std::make_pair("isObject", 1), FunctionData {Operation::IsObject}},
      {std::make_pair("isString", 1), FunctionData {Operation::IsString}},
      {std::make_pair("last", 1), FunctionData {Operation::Last}},
      {std::make_pair("length", 1), FunctionData {Operation::Length}},
      {std::make_pair("lower", 1), FunctionData {Operation::Lower}},
      {std::make_pair("max", 1), FunctionData {Operation::Max}},
      {std::make_pair("min", 1), FunctionData {Operation::Min}},
      {std::make_pair("odd", 1), FunctionData {Operation::Odd}},
      {std::make_pair("range", 1), FunctionData {Operation::Range}},
      {std::make_pair("round", 2), FunctionData {Operation::Round}},
      {std::make_pair("sort", 1), FunctionData {Operation::Sort}},
      {std::make_pair("upper", 1), FunctionData {Operation::Upper}},
      {std::make_pair("super", 0), FunctionData {Operation::Super}},
      {std::make_pair("super", 1), FunctionData {Operation::Super}},
      {std::make_pair("join", 2), FunctionData {Operation::Join}},
  };

public:
  void add_builtin(std::string_view name, int num_args, Operation op) {
    function_storage.emplace(std::make_pair(static_cast<std::string>(name), num_args), FunctionData {op});
  }

  void add_callback(std::string_view name, int num_args, const CallbackFunction& callback) {
    function_storage.emplace(std::make_pair(static_cast<std::string>(name), num_args), FunctionData {Operation::Callback, callback});
  }

  FunctionData find_function(std::string_view name, int num_args) const {
    auto it = function_storage.find(std::make_pair(static_cast<std::string>(name), num_args));
    if (it != function_storage.end()) {
      return it->second;

      // Find variadic function
    } else if (num_args > 0) {
      it = function_storage.find(std::make_pair(static_cast<std::string>(name), VARIADIC));
      if (it != function_storage.end()) {
        return it->second;
      }
    }

    return FunctionData {Operation::None};
  }
};

} // namespace inja

#endif // INCLUDE_INJA_FUNCTION_STORAGE_HPP_

// #include "utils.hpp"
#ifndef INCLUDE_INJA_UTILS_HPP_
#define INCLUDE_INJA_UTILS_HPP_

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

// #include "exceptions.hpp"
#ifndef INCLUDE_INJA_EXCEPTIONS_HPP_
#define INCLUDE_INJA_EXCEPTIONS_HPP_

#include <stdexcept>
#include <string>

namespace inja {

struct SourceLocation {
  size_t line;
  size_t column;
};

struct InjaError : public std::runtime_error {
  const std::string type;
  const std::string message;

  const SourceLocation location;

  explicit InjaError(const std::string& type, const std::string& message)
      : std::runtime_error("[inja.exception." + type + "] " + message), type(type), message(message), location({0, 0}) {}

  explicit InjaError(const std::string& type, const std::string& message, SourceLocation location)
      : std::runtime_error("[inja.exception." + type + "] (at " + std::to_string(location.line) + ":" + std::to_string(location.column) + ") " + message),
        type(type), message(message), location(location) {}
};

struct ParserError : public InjaError {
  explicit ParserError(const std::string& message, SourceLocation location): InjaError("parser_error", message, location) {}
};

struct RenderError : public InjaError {
  explicit RenderError(const std::string& message, SourceLocation location): InjaError("render_error", message, location) {}
};

struct FileError : public InjaError {
  explicit FileError(const std::string& message): InjaError("file_error", message) {}
  explicit FileError(const std::string& message, SourceLocation location): InjaError("file_error", message, location) {}
};

struct DataError : public InjaError {
  explicit DataError(const std::string& message, SourceLocation location): InjaError("data_error", message, location) {}
};

} // namespace inja

#endif // INCLUDE_INJA_EXCEPTIONS_HPP_


namespace inja {

namespace string_view {
inline std::string_view slice(std::string_view view, size_t start, size_t end) {
  start = std::min(start, view.size());
  end = std::min(std::max(start, end), view.size());
  return view.substr(start, end - start);
}

inline std::pair<std::string_view, std::string_view> split(std::string_view view, char Separator) {
  size_t idx = view.find(Separator);
  if (idx == std::string_view::npos) {
    return std::make_pair(view, std::string_view());
  }
  return std::make_pair(slice(view, 0, idx), slice(view, idx + 1, std::string_view::npos));
}

inline bool starts_with(std::string_view view, std::string_view prefix) {
  return (view.size() >= prefix.size() && view.compare(0, prefix.size(), prefix) == 0);
}
} // namespace string_view

inline SourceLocation get_source_location(std::string_view content, size_t pos) {
  // Get line and offset position (starts at 1:1)
  auto sliced = string_view::slice(content, 0, pos);
  std::size_t last_newline = sliced.rfind("\n");

  if (last_newline == std::string_view::npos) {
    return {1, sliced.length() + 1};
  }

  // Count newlines
  size_t count_lines = 0;
  size_t search_start = 0;
  while (search_start <= sliced.size()) {
    search_start = sliced.find("\n", search_start) + 1;
    if (search_start == 0) {
      break;
    }
    count_lines += 1;
  }

  return {count_lines + 1, sliced.length() - last_newline};
}

inline void replace_substring(std::string& s, const std::string& f, const std::string& t) {
  if (f.empty()) {
    return;
  }
  for (auto pos = s.find(f);            // find first occurrence of f
       pos != std::string::npos;        // make sure f was found
       s.replace(pos, f.size(), t),     // replace with t, and
       pos = s.find(f, pos + t.size())) // find next occurrence of f
  {}
}

} // namespace inja

#endif // INCLUDE_INJA_UTILS_HPP_


namespace inja {

class NodeVisitor;
class BlockNode;
class TextNode;
class ExpressionNode;
class LiteralNode;
class DataNode;
class FunctionNode;
class ExpressionListNode;
class StatementNode;
class ForStatementNode;
class ForArrayStatementNode;
class ForObjectStatementNode;
class IfStatementNode;
class IncludeStatementNode;
class ExtendsStatementNode;
class BlockStatementNode;
class SetStatementNode;

class NodeVisitor {
public:
  virtual ~NodeVisitor() = default;

  virtual void visit(const BlockNode& node) = 0;
  virtual void visit(const TextNode& node) = 0;
  virtual void visit(const ExpressionNode& node) = 0;
  virtual void visit(const LiteralNode& node) = 0;
  virtual void visit(const DataNode& node) = 0;
  virtual void visit(const FunctionNode& node) = 0;
  virtual void visit(const ExpressionListNode& node) = 0;
  virtual void visit(const StatementNode& node) = 0;
  virtual void visit(const ForStatementNode& node) = 0;
  virtual void visit(const ForArrayStatementNode& node) = 0;
  virtual void visit(const ForObjectStatementNode& node) = 0;
  virtual void visit(const IfStatementNode& node) = 0;
  virtual void visit(const IncludeStatementNode& node) = 0;
  virtual void visit(const ExtendsStatementNode& node) = 0;
  virtual void visit(const BlockStatementNode& node) = 0;
  virtual void visit(const SetStatementNode& node) = 0;
};

/*!
 * \brief Base node class for the abstract syntax tree (AST).
 */
class AstNode {
public:
  virtual void accept(NodeVisitor& v) const = 0;

  size_t pos;

  AstNode(size_t pos): pos(pos) {}
  virtual ~AstNode() {}
};

class BlockNode : public AstNode {
public:
  std::vector<std::shared_ptr<AstNode>> nodes;

  explicit BlockNode(): AstNode(0) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class TextNode : public AstNode {
public:
  const size_t length;

  explicit TextNode(size_t pos, size_t length): AstNode(pos), length(length) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class ExpressionNode : public AstNode {
public:
  explicit ExpressionNode(size_t pos): AstNode(pos) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class LiteralNode : public ExpressionNode {
public:
  const json value;

  explicit LiteralNode(std::string_view data_text, size_t pos): ExpressionNode(pos), value(json::parse(data_text)) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class DataNode : public ExpressionNode {
public:
  const std::string name;
  const json::json_pointer ptr;

  static std::string convert_dot_to_ptr(std::string_view ptr_name) {
    std::string result;
    do {
      std::string_view part;
      std::tie(part, ptr_name) = string_view::split(ptr_name, '.');
      result.push_back('/');
      result.append(part.begin(), part.end());
    } while (!ptr_name.empty());
    return result;
  }

  explicit DataNode(std::string_view ptr_name, size_t pos): ExpressionNode(pos), name(ptr_name), ptr(json::json_pointer(convert_dot_to_ptr(ptr_name))) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class FunctionNode : public ExpressionNode {
  using Op = FunctionStorage::Operation;

public:
  enum class Associativity {
    Left,
    Right,
  };

  unsigned int precedence;
  Associativity associativity;

  Op operation;

  std::string name;
  int number_args; // Should also be negative -> -1 for unknown number
  std::vector<std::shared_ptr<ExpressionNode>> arguments;
  CallbackFunction callback;

  explicit FunctionNode(std::string_view name, size_t pos)
      : ExpressionNode(pos), precedence(8), associativity(Associativity::Left), operation(Op::Callback), name(name), number_args(1) {}
  explicit FunctionNode(Op operation, size_t pos): ExpressionNode(pos), operation(operation), number_args(1) {
    switch (operation) {
    case Op::Not: {
      number_args = 1;
      precedence = 4;
      associativity = Associativity::Left;
    } break;
    case Op::And: {
      number_args = 2;
      precedence = 1;
      associativity = Associativity::Left;
    } break;
    case Op::Or: {
      number_args = 2;
      precedence = 1;
      associativity = Associativity::Left;
    } break;
    case Op::In: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::Equal: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::NotEqual: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::Greater: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::GreaterEqual: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::Less: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::LessEqual: {
      number_args = 2;
      precedence = 2;
      associativity = Associativity::Left;
    } break;
    case Op::Add: {
      number_args = 2;
      precedence = 3;
      associativity = Associativity::Left;
    } break;
    case Op::Subtract: {
      number_args = 2;
      precedence = 3;
      associativity = Associativity::Left;
    } break;
    case Op::Multiplication: {
      number_args = 2;
      precedence = 4;
      associativity = Associativity::Left;
    } break;
    case Op::Division: {
      number_args = 2;
      precedence = 4;
      associativity = Associativity::Left;
    } break;
    case Op::Power: {
      number_args = 2;
      precedence = 5;
      associativity = Associativity::Right;
    } break;
    case Op::Modulo: {
      number_args = 2;
      precedence = 4;
      associativity = Associativity::Left;
    } break;
    case Op::AtId: {
      number_args = 2;
      precedence = 8;
      associativity = Associativity::Left;
    } break;
    default: {
      precedence = 1;
      associativity = Associativity::Left;
    }
    }
  }

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class ExpressionListNode : public AstNode {
public:
  std::shared_ptr<ExpressionNode> root;

  explicit ExpressionListNode(): AstNode(0) {}
  explicit ExpressionListNode(size_t pos): AstNode(pos) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class StatementNode : public AstNode {
public:
  StatementNode(size_t pos): AstNode(pos) {}

  virtual void accept(NodeVisitor& v) const = 0;
};

class ForStatementNode : public StatementNode {
public:
  ExpressionListNode condition;
  BlockNode body;
  BlockNode* const parent;

  ForStatementNode(BlockNode* const parent, size_t pos): StatementNode(pos), parent(parent) {}

  virtual void accept(NodeVisitor& v) const = 0;
};

class ForArrayStatementNode : public ForStatementNode {
public:
  const std::string value;

  explicit ForArrayStatementNode(const std::string& value, BlockNode* const parent, size_t pos): ForStatementNode(parent, pos), value(value) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class ForObjectStatementNode : public ForStatementNode {
public:
  const std::string key;
  const std::string value;

  explicit ForObjectStatementNode(const std::string& key, const std::string& value, BlockNode* const parent, size_t pos)
      : ForStatementNode(parent, pos), key(key), value(value) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class IfStatementNode : public StatementNode {
public:
  ExpressionListNode condition;
  BlockNode true_statement;
  BlockNode false_statement;
  BlockNode* const parent;

  const bool is_nested;
  bool has_false_statement {false};

  explicit IfStatementNode(BlockNode* const parent, size_t pos): StatementNode(pos), parent(parent), is_nested(false) {}
  explicit IfStatementNode(bool is_nested, BlockNode* const parent, size_t pos): StatementNode(pos), parent(parent), is_nested(is_nested) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class IncludeStatementNode : public StatementNode {
public:
  const std::string file;

  explicit IncludeStatementNode(const std::string& file, size_t pos): StatementNode(pos), file(file) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

class ExtendsStatementNode : public StatementNode {
public:
  const std::string file;

  explicit ExtendsStatementNode(const std::string& file, size_t pos): StatementNode(pos), file(file) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  };
};

class BlockStatementNode : public StatementNode {
public:
  const std::string name;
  BlockNode block;
  BlockNode* const parent;

  explicit BlockStatementNode(BlockNode* const parent, const std::string& name, size_t pos): StatementNode(pos), name(name), parent(parent) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  };
};

class SetStatementNode : public StatementNode {
public:
  const std::string key;
  ExpressionListNode expression;

  explicit SetStatementNode(const std::string& key, size_t pos): StatementNode(pos), key(key) {}

  void accept(NodeVisitor& v) const {
    v.visit(*this);
  }
};

} // namespace inja

#endif // INCLUDE_INJA_NODE_HPP_

// #include "statistics.hpp"
#ifndef INCLUDE_INJA_STATISTICS_HPP_
#define INCLUDE_INJA_STATISTICS_HPP_

// #include "node.hpp"


namespace inja {

/*!
 * \brief A class for counting statistics on a Template.
 */
class StatisticsVisitor : public NodeVisitor {
  void visit(const BlockNode& node) {
    for (auto& n : node.nodes) {
      n->accept(*this);
    }
  }

  void visit(const TextNode&) {}
  void visit(const ExpressionNode&) {}
  void visit(const LiteralNode&) {}

  void visit(const DataNode&) {
    variable_counter += 1;
  }

  void visit(const FunctionNode& node) {
    for (auto& n : node.arguments) {
      n->accept(*this);
    }
  }

  void visit(const ExpressionListNode& node) {
    node.root->accept(*this);
  }

  void visit(const StatementNode&) {}
  void visit(const ForStatementNode&) {}

  void visit(const ForArrayStatementNode& node) {
    node.condition.accept(*this);
    node.body.accept(*this);
  }

  void visit(const ForObjectStatementNode& node) {
    node.condition.accept(*this);
    node.body.accept(*this);
  }

  void visit(const IfStatementNode& node) {
    node.condition.accept(*this);
    node.true_statement.accept(*this);
    node.false_statement.accept(*this);
  }

  void visit(const IncludeStatementNode&) {}

  void visit(const ExtendsStatementNode&) {}

  void visit(const BlockStatementNode& node) {
    node.block.accept(*this);
  }

  void visit(const SetStatementNode&) {}

public:
  unsigned int variable_counter;

  explicit StatisticsVisitor(): variable_counter(0) {}
};

} // namespace inja

#endif // INCLUDE_INJA_STATISTICS_HPP_


namespace inja {

/*!
 * \brief The main inja Template.
 */
struct Template {
  BlockNode root;
  std::string content;
  std::map<std::string, std::shared_ptr<BlockStatementNode>> block_storage;

  explicit Template() {}
  explicit Template(const std::string& content): content(content) {}

  /// Return number of variables (total number, not distinct ones) in the template
  int count_variables() {
    auto statistic_visitor = StatisticsVisitor();
    root.accept(statistic_visitor);
    return statistic_visitor.variable_counter;
  }
};

using TemplateStorage = std::map<std::string, Template>;

} // namespace inja

#endif // INCLUDE_INJA_TEMPLATE_HPP_


namespace inja {

/*!
 * \brief Class for lexer configuration.
 */
struct LexerConfig {
  std::string statement_open {"{%"};
  std::string statement_open_no_lstrip {"{%+"};
  std::string statement_open_force_lstrip {"{%-"};
  std::string statement_close {"%}"};
  std::string statement_close_force_rstrip {"-%}"};
  std::string line_statement {"##"};
  std::string expression_open {"{{"};
  std::string expression_open_force_lstrip {"{{-"};
  std::string expression_close {"}}"};
  std::string expression_close_force_rstrip {"-}}"};
  std::string comment_open {"{#"};
  std::string comment_open_force_lstrip {"{#-"};
  std::string comment_close {"#}"};
  std::string comment_close_force_rstrip {"-#}"};
  std::string open_chars {"#{"};

  bool trim_blocks {false};
  bool lstrip_blocks {false};

  void update_open_chars() {
    open_chars = "";
    if (open_chars.find(line_statement[0]) == std::string::npos) {
      open_chars += line_statement[0];
    }
    if (open_chars.find(statement_open[0]) == std::string::npos) {
      open_chars += statement_open[0];
    }
    if (open_chars.find(statement_open_no_lstrip[0]) == std::string::npos) {
      open_chars += statement_open_no_lstrip[0];
    }
    if (open_chars.find(statement_open_force_lstrip[0]) == std::string::npos) {
      open_chars += statement_open_force_lstrip[0];
    }
    if (open_chars.find(expression_open[0]) == std::string::npos) {
      open_chars += expression_open[0];
    }
    if (open_chars.find(expression_open_force_lstrip[0]) == std::string::npos) {
      open_chars += expression_open_force_lstrip[0];
    }
    if (open_chars.find(comment_open[0]) == std::string::npos) {
      open_chars += comment_open[0];
    }
    if (open_chars.find(comment_open_force_lstrip[0]) == std::string::npos) {
      open_chars += comment_open_force_lstrip[0];
    }
  }
};

/*!
 * \brief Class for parser configuration.
 */
struct ParserConfig {
  bool search_included_templates_in_files {true};

  std::function<Template(const std::string&, const std::string&)> include_callback;
};

/*!
 * \brief Class for render configuration.
 */
struct RenderConfig {
  bool throw_at_missing_includes {true};
};

} // namespace inja

#endif // INCLUDE_INJA_CONFIG_HPP_

// #include "function_storage.hpp"

// #include "parser.hpp"
#ifndef INCLUDE_INJA_PARSER_HPP_
#define INCLUDE_INJA_PARSER_HPP_

#include <limits>
#include <stack>
#include <string>
#include <utility>
#include <vector>

// #include "config.hpp"

// #include "exceptions.hpp"

// #include "function_storage.hpp"

// #include "lexer.hpp"
#ifndef INCLUDE_INJA_LEXER_HPP_
#define INCLUDE_INJA_LEXER_HPP_

#include <cctype>
#include <locale>

// #include "config.hpp"

// #include "token.hpp"
#ifndef INCLUDE_INJA_TOKEN_HPP_
#define INCLUDE_INJA_TOKEN_HPP_

#include <string>
#include <string_view>

namespace inja {

/*!
 * \brief Helper-class for the inja Lexer.
 */
struct Token {
  enum class Kind {
    Text,
    ExpressionOpen,     // {{
    ExpressionClose,    // }}
    LineStatementOpen,  // ##
    LineStatementClose, // \n
    StatementOpen,      // {%
    StatementClose,     // %}
    CommentOpen,        // {#
    CommentClose,       // #}
    Id,                 // this, this.foo
    Number,             // 1, 2, -1, 5.2, -5.3
    String,             // "this"
    Plus,               // +
    Minus,              // -
    Times,              // *
    Slash,              // /
    Percent,            // %
    Power,              // ^
    Comma,              // ,
    Dot,                // .
    Colon,              // :
    LeftParen,          // (
    RightParen,         // )
    LeftBracket,        // [
    RightBracket,       // ]
    LeftBrace,          // {
    RightBrace,         // }
    Equal,              // ==
    NotEqual,           // !=
    GreaterThan,        // >
    GreaterEqual,       // >=
    LessThan,           // <
    LessEqual,          // <=
    Unknown,
    Eof,
  };

  Kind kind {Kind::Unknown};
  std::string_view text;

  explicit constexpr Token() = default;
  explicit constexpr Token(Kind kind, std::string_view text): kind(kind), text(text) {}

  std::string describe() const {
    switch (kind) {
    case Kind::Text:
      return "<text>";
    case Kind::LineStatementClose:
      return "<eol>";
    case Kind::Eof:
      return "<eof>";
    default:
      return static_cast<std::string>(text);
    }
  }
};

} // namespace inja

#endif // INCLUDE_INJA_TOKEN_HPP_

// #include "utils.hpp"


namespace inja {

/*!
 * \brief Class for lexing an inja Template.
 */
class Lexer {
  enum class State {
    Text,
    ExpressionStart,
    ExpressionStartForceLstrip,
    ExpressionBody,
    LineStart,
    LineBody,
    StatementStart,
    StatementStartNoLstrip,
    StatementStartForceLstrip,
    StatementBody,
    CommentStart,
    CommentStartForceLstrip,
    CommentBody,
  };

  enum class MinusState {
    Operator,
    Number,
  };

  const LexerConfig& config;

  State state;
  MinusState minus_state;
  std::string_view m_in;
  size_t tok_start;
  size_t pos;

  Token scan_body(std::string_view close, Token::Kind closeKind, std::string_view close_trim = std::string_view(), bool trim = false) {
  again:
    // skip whitespace (except for \n as it might be a close)
    if (tok_start >= m_in.size()) {
      return make_token(Token::Kind::Eof);
    }
    const char ch = m_in[tok_start];
    if (ch == ' ' || ch == '\t' || ch == '\r') {
      tok_start += 1;
      goto again;
    }

    // check for close
    if (!close_trim.empty() && inja::string_view::starts_with(m_in.substr(tok_start), close_trim)) {
      state = State::Text;
      pos = tok_start + close_trim.size();
      const Token tok = make_token(closeKind);
      skip_whitespaces_and_newlines();
      return tok;
    }

    if (inja::string_view::starts_with(m_in.substr(tok_start), close)) {
      state = State::Text;
      pos = tok_start + close.size();
      const Token tok = make_token(closeKind);
      if (trim) {
        skip_whitespaces_and_first_newline();
      }
      return tok;
    }

    // skip \n
    if (ch == '\n') {
      tok_start += 1;
      goto again;
    }

    pos = tok_start + 1;
    if (std::isalpha(ch)) {
      minus_state = MinusState::Operator;
      return scan_id();
    }

    const MinusState current_minus_state = minus_state;
    if (minus_state == MinusState::Operator) {
      minus_state = MinusState::Number;
    }

    switch (ch) {
    case '+':
      return make_token(Token::Kind::Plus);
    case '-':
      if (current_minus_state == MinusState::Operator) {
        return make_token(Token::Kind::Minus);
      }
      return scan_number();
    case '*':
      return make_token(Token::Kind::Times);
    case '/':
      return make_token(Token::Kind::Slash);
    case '^':
      return make_token(Token::Kind::Power);
    case '%':
      return make_token(Token::Kind::Percent);
    case '.':
      return make_token(Token::Kind::Dot);
    case ',':
      return make_token(Token::Kind::Comma);
    case ':':
      return make_token(Token::Kind::Colon);
    case '(':
      return make_token(Token::Kind::LeftParen);
    case ')':
      minus_state = MinusState::Operator;
      return make_token(Token::Kind::RightParen);
    case '[':
      return make_token(Token::Kind::LeftBracket);
    case ']':
      minus_state = MinusState::Operator;
      return make_token(Token::Kind::RightBracket);
    case '{':
      return make_token(Token::Kind::LeftBrace);
    case '}':
      minus_state = MinusState::Operator;
      return make_token(Token::Kind::RightBrace);
    case '>':
      if (pos < m_in.size() && m_in[pos] == '=') {
        pos += 1;
        return make_token(Token::Kind::GreaterEqual);
      }
      return make_token(Token::Kind::GreaterThan);
    case '<':
      if (pos < m_in.size() && m_in[pos] == '=') {
        pos += 1;
        return make_token(Token::Kind::LessEqual);
      }
      return make_token(Token::Kind::LessThan);
    case '=':
      if (pos < m_in.size() && m_in[pos] == '=') {
        pos += 1;
        return make_token(Token::Kind::Equal);
      }
      return make_token(Token::Kind::Unknown);
    case '!':
      if (pos < m_in.size() && m_in[pos] == '=') {
        pos += 1;
        return make_token(Token::Kind::NotEqual);
      }
      return make_token(Token::Kind::Unknown);
    case '\"':
      return scan_string();
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      minus_state = MinusState::Operator;
      return scan_number();
    case '_':
    case '@':
    case '$':
      minus_state = MinusState::Operator;
      return scan_id();
    default:
      return make_token(Token::Kind::Unknown);
    }
  }

  Token scan_id() {
    for (;;) {
      if (pos >= m_in.size()) {
        break;
      }
      const char ch = m_in[pos];
      if (!std::isalnum(ch) && ch != '.' && ch != '/' && ch != '_' && ch != '-') {
        break;
      }
      pos += 1;
    }
    return make_token(Token::Kind::Id);
  }

  Token scan_number() {
    for (;;) {
      if (pos >= m_in.size()) {
        break;
      }
      const char ch = m_in[pos];
      // be very permissive in lexer (we'll catch errors when conversion happens)
      if (!(std::isdigit(ch) || ch == '.' || ch == 'e' || ch == 'E' || (ch == '+' && (pos == 0 || m_in[pos-1] == 'e' || m_in[pos-1] == 'E')) || (ch == '-' && (pos == 0 || m_in[pos-1] == 'e' || m_in[pos-1] == 'E')))) {
        break;
      }
      pos += 1;
    }
    return make_token(Token::Kind::Number);
  }

  Token scan_string() {
    bool escape {false};
    for (;;) {
      if (pos >= m_in.size()) {
        break;
      }
      const char ch = m_in[pos++];
      if (ch == '\\') {
        escape = true;
      } else if (!escape && ch == m_in[tok_start]) {
        break;
      } else {
        escape = false;
      }
    }
    return make_token(Token::Kind::String);
  }

  Token make_token(Token::Kind kind) const {
    return Token(kind, string_view::slice(m_in, tok_start, pos));
  }

  void skip_whitespaces_and_newlines() {
    if (pos < m_in.size()) {
      while (pos < m_in.size() && (m_in[pos] == ' ' || m_in[pos] == '\t' || m_in[pos] == '\n' || m_in[pos] == '\r')) {
        pos += 1;
      }
    }
  }

  void skip_whitespaces_and_first_newline() {
    if (pos < m_in.size()) {
      while (pos < m_in.size() && (m_in[pos] == ' ' || m_in[pos] == '\t')) {
        pos += 1;
      }
    }

    if (pos < m_in.size()) {
      const char ch = m_in[pos];
      if (ch == '\n') {
        pos += 1;
      } else if (ch == '\r') {
        pos += 1;
        if (pos < m_in.size() && m_in[pos] == '\n') {
          pos += 1;
        }
      }
    }
  }

  static std::string_view clear_final_line_if_whitespace(std::string_view text) {
    std::string_view result = text;
    while (!result.empty()) {
      const char ch = result.back();
      if (ch == ' ' || ch == '\t') {
        result.remove_suffix(1);
      } else if (ch == '\n' || ch == '\r') {
        break;
      } else {
        return text;
      }
    }
    return result;
  }

public:
  explicit Lexer(const LexerConfig& config): config(config), state(State::Text), minus_state(MinusState::Number) {}

  SourceLocation current_position() const {
    return get_source_location(m_in, tok_start);
  }

  void start(std::string_view input) {
    m_in = input;
    tok_start = 0;
    pos = 0;
    state = State::Text;
    minus_state = MinusState::Number;

    // Consume byte order mark (BOM) for UTF-8
    if (inja::string_view::starts_with(m_in, "\xEF\xBB\xBF")) {
      m_in = m_in.substr(3);
    }
  }

  Token scan() {
    tok_start = pos;

  again:
    if (tok_start >= m_in.size()) {
      return make_token(Token::Kind::Eof);
    }

    switch (state) {
    default:
    case State::Text: {
      // fast-scan to first open character
      const size_t open_start = m_in.substr(pos).find_first_of(config.open_chars);
      if (open_start == std::string_view::npos) {
        // didn't find open, return remaining text as text token
        pos = m_in.size();
        return make_token(Token::Kind::Text);
      }
      pos += open_start;

      // try to match one of the opening sequences, and get the close
      std::string_view open_str = m_in.substr(pos);
      bool must_lstrip = false;
      if (inja::string_view::starts_with(open_str, config.expression_open)) {
        if (inja::string_view::starts_with(open_str, config.expression_open_force_lstrip)) {
          state = State::ExpressionStartForceLstrip;
          must_lstrip = true;
        } else {
          state = State::ExpressionStart;
        }
      } else if (inja::string_view::starts_with(open_str, config.statement_open)) {
        if (inja::string_view::starts_with(open_str, config.statement_open_no_lstrip)) {
          state = State::StatementStartNoLstrip;
        } else if (inja::string_view::starts_with(open_str, config.statement_open_force_lstrip)) {
          state = State::StatementStartForceLstrip;
          must_lstrip = true;
        } else {
          state = State::StatementStart;
          must_lstrip = config.lstrip_blocks;
        }
      } else if (inja::string_view::starts_with(open_str, config.comment_open)) {
        if (inja::string_view::starts_with(open_str, config.comment_open_force_lstrip)) {
          state = State::CommentStartForceLstrip;
          must_lstrip = true;
        } else {
          state = State::CommentStart;
          must_lstrip = config.lstrip_blocks;
        }
      } else if ((pos == 0 || m_in[pos - 1] == '\n') && inja::string_view::starts_with(open_str, config.line_statement)) {
        state = State::LineStart;
      } else {
        pos += 1; // wasn't actually an opening sequence
        goto again;
      }

      std::string_view text = string_view::slice(m_in, tok_start, pos);
      if (must_lstrip) {
        text = clear_final_line_if_whitespace(text);
      }

      if (text.empty()) {
        goto again; // don't generate empty token
      }
      return Token(Token::Kind::Text, text);
    }
    case State::ExpressionStart: {
      state = State::ExpressionBody;
      pos += config.expression_open.size();
      return make_token(Token::Kind::ExpressionOpen);
    }
    case State::ExpressionStartForceLstrip: {
      state = State::ExpressionBody;
      pos += config.expression_open_force_lstrip.size();
      return make_token(Token::Kind::ExpressionOpen);
    }
    case State::LineStart: {
      state = State::LineBody;
      pos += config.line_statement.size();
      return make_token(Token::Kind::LineStatementOpen);
    }
    case State::StatementStart: {
      state = State::StatementBody;
      pos += config.statement_open.size();
      return make_token(Token::Kind::StatementOpen);
    }
    case State::StatementStartNoLstrip: {
      state = State::StatementBody;
      pos += config.statement_open_no_lstrip.size();
      return make_token(Token::Kind::StatementOpen);
    }
    case State::StatementStartForceLstrip: {
      state = State::StatementBody;
      pos += config.statement_open_force_lstrip.size();
      return make_token(Token::Kind::StatementOpen);
    }
    case State::CommentStart: {
      state = State::CommentBody;
      pos += config.comment_open.size();
      return make_token(Token::Kind::CommentOpen);
    }
    case State::CommentStartForceLstrip: {
      state = State::CommentBody;
      pos += config.comment_open_force_lstrip.size();
      return make_token(Token::Kind::CommentOpen);
    }
    case State::ExpressionBody:
      return scan_body(config.expression_close, Token::Kind::ExpressionClose, config.expression_close_force_rstrip);
    case State::LineBody:
      return scan_body("\n", Token::Kind::LineStatementClose);
    case State::StatementBody:
      return scan_body(config.statement_close, Token::Kind::StatementClose, config.statement_close_force_rstrip, config.trim_blocks);
    case State::CommentBody: {
      // fast-scan to comment close
      const size_t end = m_in.substr(pos).find(config.comment_close);
      if (end == std::string_view::npos) {
        pos = m_in.size();
        return make_token(Token::Kind::Eof);
      }

      // Check for trim pattern
      const bool must_rstrip = inja::string_view::starts_with(m_in.substr(pos + end - 1), config.comment_close_force_rstrip);

      // return the entire comment in the close token
      state = State::Text;
      pos += end + config.comment_close.size();
      Token tok = make_token(Token::Kind::CommentClose);

      if (must_rstrip || config.trim_blocks) {
        skip_whitespaces_and_first_newline();
      }
      return tok;
    }
    }
  }

  const LexerConfig& get_config() const {
    return config;
  }
};

} // namespace inja

#endif // INCLUDE_INJA_LEXER_HPP_

// #include "node.hpp"

// #include "template.hpp"

// #include "token.hpp"

// #include "utils.hpp"


namespace inja {

/*!
 * \brief Class for parsing an inja Template.
 */
class Parser {
  const ParserConfig& config;

  Lexer lexer;
  TemplateStorage& template_storage;
  const FunctionStorage& function_storage;

  Token tok, peek_tok;
  bool have_peek_tok {false};

  size_t current_paren_level {0};
  size_t current_bracket_level {0};
  size_t current_brace_level {0};

  std::string_view literal_start;

  BlockNode* current_block {nullptr};
  ExpressionListNode* current_expression_list {nullptr};
  std::stack<std::pair<FunctionNode*, size_t>> function_stack;
  std::vector<std::shared_ptr<ExpressionNode>> arguments;

  std::stack<std::shared_ptr<FunctionNode>> operator_stack;
  std::stack<IfStatementNode*> if_statement_stack;
  std::stack<ForStatementNode*> for_statement_stack;
  std::stack<BlockStatementNode*> block_statement_stack;

  inline void throw_parser_error(const std::string& message) const {
    INJA_THROW(ParserError(message, lexer.current_position()));
  }

  inline void get_next_token() {
    if (have_peek_tok) {
      tok = peek_tok;
      have_peek_tok = false;
    } else {
      tok = lexer.scan();
    }
  }

  inline void get_peek_token() {
    if (!have_peek_tok) {
      peek_tok = lexer.scan();
      have_peek_tok = true;
    }
  }

  inline void add_literal(const char* content_ptr) {
    std::string_view data_text(literal_start.data(), tok.text.data() - literal_start.data() + tok.text.size());
    arguments.emplace_back(std::make_shared<LiteralNode>(data_text, data_text.data() - content_ptr));
  }

  inline void add_operator() {
    auto function = operator_stack.top();
    operator_stack.pop();

    for (int i = 0; i < function->number_args; ++i) {
      function->arguments.insert(function->arguments.begin(), arguments.back());
      arguments.pop_back();
    }
    arguments.emplace_back(function);
  }

  void add_to_template_storage(std::string_view path, std::string& template_name) {
    if (template_storage.find(template_name) != template_storage.end()) {
      return;
    }

    std::string original_path = static_cast<std::string>(path);
    std::string original_name = template_name;

    if (config.search_included_templates_in_files) {
      // Build the relative path
      template_name = original_path + original_name;
      if (template_name.compare(0, 2, "./") == 0) {
        template_name.erase(0, 2);
      }

      if (template_storage.find(template_name) == template_storage.end()) {
        // Load file
        std::ifstream file;
        file.open(template_name);
        if (!file.fail()) {
          std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

          auto include_template = Template(text);
          template_storage.emplace(template_name, include_template);
          parse_into_template(template_storage[template_name], template_name);
          return;
        } else if (!config.include_callback) {
          INJA_THROW(FileError("failed accessing file at '" + template_name + "'"));
        }
      }
    }

    // Try include callback
    if (config.include_callback) {
      auto include_template = config.include_callback(original_path, original_name);
      template_storage.emplace(template_name, include_template);
    }
  }

  std::string parse_filename(const Token& tok) const {
    if (tok.kind != Token::Kind::String) {
      throw_parser_error("expected string, got '" + tok.describe() + "'");
    }

    if (tok.text.length() < 2) {
      throw_parser_error("expected filename, got '" + static_cast<std::string>(tok.text) + "'");
    }

    // Remove first and last character ""
    return std::string {tok.text.substr(1, tok.text.length() - 2)};
  }

  bool parse_expression(Template& tmpl, Token::Kind closing) {
    while (tok.kind != closing && tok.kind != Token::Kind::Eof) {
      // Literals
      switch (tok.kind) {
      case Token::Kind::String: {
        if (current_brace_level == 0 && current_bracket_level == 0) {
          literal_start = tok.text;
          add_literal(tmpl.content.c_str());
        }
      } break;
      case Token::Kind::Number: {
        if (current_brace_level == 0 && current_bracket_level == 0) {
          literal_start = tok.text;
          add_literal(tmpl.content.c_str());
        }
      } break;
      case Token::Kind::LeftBracket: {
        if (current_brace_level == 0 && current_bracket_level == 0) {
          literal_start = tok.text;
        }
        current_bracket_level += 1;
      } break;
      case Token::Kind::LeftBrace: {
        if (current_brace_level == 0 && current_bracket_level == 0) {
          literal_start = tok.text;
        }
        current_brace_level += 1;
      } break;
      case Token::Kind::RightBracket: {
        if (current_bracket_level == 0) {
          throw_parser_error("unexpected ']'");
        }

        current_bracket_level -= 1;
        if (current_brace_level == 0 && current_bracket_level == 0) {
          add_literal(tmpl.content.c_str());
        }
      } break;
      case Token::Kind::RightBrace: {
        if (current_brace_level == 0) {
          throw_parser_error("unexpected '}'");
        }

        current_brace_level -= 1;
        if (current_brace_level == 0 && current_bracket_level == 0) {
          add_literal(tmpl.content.c_str());
        }
      } break;
      case Token::Kind::Id: {
        get_peek_token();

        // Data Literal
        if (tok.text == static_cast<decltype(tok.text)>("true") || tok.text == static_cast<decltype(tok.text)>("false") ||
            tok.text == static_cast<decltype(tok.text)>("null")) {
          if (current_brace_level == 0 && current_bracket_level == 0) {
            literal_start = tok.text;
            add_literal(tmpl.content.c_str());
          }

          // Operator
        } else if (tok.text == "and" || tok.text == "or" || tok.text == "in" || tok.text == "not") {
          goto parse_operator;

          // Functions
        } else if (peek_tok.kind == Token::Kind::LeftParen) {
          operator_stack.emplace(std::make_shared<FunctionNode>(static_cast<std::string>(tok.text), tok.text.data() - tmpl.content.c_str()));
          function_stack.emplace(operator_stack.top().get(), current_paren_level);

          // Variables
        } else {
          arguments.emplace_back(std::make_shared<DataNode>(static_cast<std::string>(tok.text), tok.text.data() - tmpl.content.c_str()));
        }

        // Operators
      } break;
      case Token::Kind::Equal:
      case Token::Kind::NotEqual:
      case Token::Kind::GreaterThan:
      case Token::Kind::GreaterEqual:
      case Token::Kind::LessThan:
      case Token::Kind::LessEqual:
      case Token::Kind::Plus:
      case Token::Kind::Minus:
      case Token::Kind::Times:
      case Token::Kind::Slash:
      case Token::Kind::Power:
      case Token::Kind::Percent:
      case Token::Kind::Dot: {

      parse_operator:
        FunctionStorage::Operation operation;
        switch (tok.kind) {
        case Token::Kind::Id: {
          if (tok.text == "and") {
            operation = FunctionStorage::Operation::And;
          } else if (tok.text == "or") {
            operation = FunctionStorage::Operation::Or;
          } else if (tok.text == "in") {
            operation = FunctionStorage::Operation::In;
          } else if (tok.text == "not") {
            operation = FunctionStorage::Operation::Not;
          } else {
            throw_parser_error("unknown operator in parser.");
          }
        } break;
        case Token::Kind::Equal: {
          operation = FunctionStorage::Operation::Equal;
        } break;
        case Token::Kind::NotEqual: {
          operation = FunctionStorage::Operation::NotEqual;
        } break;
        case Token::Kind::GreaterThan: {
          operation = FunctionStorage::Operation::Greater;
        } break;
        case Token::Kind::GreaterEqual: {
          operation = FunctionStorage::Operation::GreaterEqual;
        } break;
        case Token::Kind::LessThan: {
          operation = FunctionStorage::Operation::Less;
        } break;
        case Token::Kind::LessEqual: {
          operation = FunctionStorage::Operation::LessEqual;
        } break;
        case Token::Kind::Plus: {
          operation = FunctionStorage::Operation::Add;
        } break;
        case Token::Kind::Minus: {
          operation = FunctionStorage::Operation::Subtract;
        } break;
        case Token::Kind::Times: {
          operation = FunctionStorage::Operation::Multiplication;
        } break;
        case Token::Kind::Slash: {
          operation = FunctionStorage::Operation::Division;
        } break;
        case Token::Kind::Power: {
          operation = FunctionStorage::Operation::Power;
        } break;
        case Token::Kind::Percent: {
          operation = FunctionStorage::Operation::Modulo;
        } break;
        case Token::Kind::Dot: {
          operation = FunctionStorage::Operation::AtId;
        } break;
        default: {
          throw_parser_error("unknown operator in parser.");
        }
        }
        auto function_node = std::make_shared<FunctionNode>(operation, tok.text.data() - tmpl.content.c_str());

        while (!operator_stack.empty() &&
               ((operator_stack.top()->precedence > function_node->precedence) ||
                (operator_stack.top()->precedence == function_node->precedence && function_node->associativity == FunctionNode::Associativity::Left)) &&
               (operator_stack.top()->operation != FunctionStorage::Operation::ParenLeft)) {
          add_operator();
        }

        operator_stack.emplace(function_node);
      } break;
      case Token::Kind::Comma: {
        if (current_brace_level == 0 && current_bracket_level == 0) {
          if (function_stack.empty()) {
            throw_parser_error("unexpected ','");
          }

          function_stack.top().first->number_args += 1;
        }
      } break;
      case Token::Kind::Colon: {
        if (current_brace_level == 0 && current_bracket_level == 0) {
          throw_parser_error("unexpected ':'");
        }
      } break;
      case Token::Kind::LeftParen: {
        current_paren_level += 1;
        operator_stack.emplace(std::make_shared<FunctionNode>(FunctionStorage::Operation::ParenLeft, tok.text.data() - tmpl.content.c_str()));

        get_peek_token();
        if (peek_tok.kind == Token::Kind::RightParen) {
          if (!function_stack.empty() && function_stack.top().second == current_paren_level - 1) {
            function_stack.top().first->number_args = 0;
          }
        }
      } break;
      case Token::Kind::RightParen: {
        current_paren_level -= 1;
        while (!operator_stack.empty() && operator_stack.top()->operation != FunctionStorage::Operation::ParenLeft) {
          add_operator();
        }

        if (!operator_stack.empty() && operator_stack.top()->operation == FunctionStorage::Operation::ParenLeft) {
          operator_stack.pop();
        }

        if (!function_stack.empty() && function_stack.top().second == current_paren_level) {
          auto func = function_stack.top().first;
          auto function_data = function_storage.find_function(func->name, func->number_args);
          if (function_data.operation == FunctionStorage::Operation::None) {
            throw_parser_error("unknown function " + func->name);
          }
          func->operation = function_data.operation;
          if (function_data.operation == FunctionStorage::Operation::Callback) {
            func->callback = function_data.callback;
          }

          if (operator_stack.empty()) {
            throw_parser_error("internal error at function " + func->name);
          }

          add_operator();
          function_stack.pop();
        }
      }
      default:
        break;
      }

      get_next_token();
    }

    while (!operator_stack.empty()) {
      add_operator();
    }

    if (arguments.size() == 1) {
      current_expression_list->root = arguments[0];
      arguments = {};
    } else if (arguments.size() > 1) {
      throw_parser_error("malformed expression");
    }

    return true;
  }

  bool parse_statement(Template& tmpl, Token::Kind closing, std::string_view path) {
    if (tok.kind != Token::Kind::Id) {
      return false;
    }

    if (tok.text == static_cast<decltype(tok.text)>("if")) {
      get_next_token();

      auto if_statement_node = std::make_shared<IfStatementNode>(current_block, tok.text.data() - tmpl.content.c_str());
      current_block->nodes.emplace_back(if_statement_node);
      if_statement_stack.emplace(if_statement_node.get());
      current_block = &if_statement_node->true_statement;
      current_expression_list = &if_statement_node->condition;

      if (!parse_expression(tmpl, closing)) {
        return false;
      }
    } else if (tok.text == static_cast<decltype(tok.text)>("else")) {
      if (if_statement_stack.empty()) {
        throw_parser_error("else without matching if");
      }
      auto& if_statement_data = if_statement_stack.top();
      get_next_token();

      if_statement_data->has_false_statement = true;
      current_block = &if_statement_data->false_statement;

      // Chained else if
      if (tok.kind == Token::Kind::Id && tok.text == static_cast<decltype(tok.text)>("if")) {
        get_next_token();

        auto if_statement_node = std::make_shared<IfStatementNode>(true, current_block, tok.text.data() - tmpl.content.c_str());
        current_block->nodes.emplace_back(if_statement_node);
        if_statement_stack.emplace(if_statement_node.get());
        current_block = &if_statement_node->true_statement;
        current_expression_list = &if_statement_node->condition;

        if (!parse_expression(tmpl, closing)) {
          return false;
        }
      }
    } else if (tok.text == static_cast<decltype(tok.text)>("endif")) {
      if (if_statement_stack.empty()) {
        throw_parser_error("endif without matching if");
      }

      // Nested if statements
      while (if_statement_stack.top()->is_nested) {
        if_statement_stack.pop();
      }

      auto& if_statement_data = if_statement_stack.top();
      get_next_token();

      current_block = if_statement_data->parent;
      if_statement_stack.pop();
    } else if (tok.text == static_cast<decltype(tok.text)>("block")) {
      get_next_token();

      if (tok.kind != Token::Kind::Id) {
        throw_parser_error("expected block name, got '" + tok.describe() + "'");
      }

      const std::string block_name = static_cast<std::string>(tok.text);

      auto block_statement_node = std::make_shared<BlockStatementNode>(current_block, block_name, tok.text.data() - tmpl.content.c_str());
      current_block->nodes.emplace_back(block_statement_node);
      block_statement_stack.emplace(block_statement_node.get());
      current_block = &block_statement_node->block;
      auto success = tmpl.block_storage.emplace(block_name, block_statement_node);
      if (!success.second) {
        throw_parser_error("block with the name '" + block_name + "' does already exist");
      }

      get_next_token();
    } else if (tok.text == static_cast<decltype(tok.text)>("endblock")) {
      if (block_statement_stack.empty()) {
        throw_parser_error("endblock without matching block");
      }

      auto& block_statement_data = block_statement_stack.top();
      get_next_token();

      current_block = block_statement_data->parent;
      block_statement_stack.pop();
    } else if (tok.text == static_cast<decltype(tok.text)>("for")) {
      get_next_token();

      // options: for a in arr; for a, b in obj
      if (tok.kind != Token::Kind::Id) {
        throw_parser_error("expected id, got '" + tok.describe() + "'");
      }

      Token value_token = tok;
      get_next_token();

      // Object type
      std::shared_ptr<ForStatementNode> for_statement_node;
      if (tok.kind == Token::Kind::Comma) {
        get_next_token();
        if (tok.kind != Token::Kind::Id) {
          throw_parser_error("expected id, got '" + tok.describe() + "'");
        }

        Token key_token = std::move(value_token);
        value_token = tok;
        get_next_token();

        for_statement_node = std::make_shared<ForObjectStatementNode>(static_cast<std::string>(key_token.text), static_cast<std::string>(value_token.text),
                                                                      current_block, tok.text.data() - tmpl.content.c_str());

        // Array type
      } else {
        for_statement_node =
            std::make_shared<ForArrayStatementNode>(static_cast<std::string>(value_token.text), current_block, tok.text.data() - tmpl.content.c_str());
      }

      current_block->nodes.emplace_back(for_statement_node);
      for_statement_stack.emplace(for_statement_node.get());
      current_block = &for_statement_node->body;
      current_expression_list = &for_statement_node->condition;

      if (tok.kind != Token::Kind::Id || tok.text != static_cast<decltype(tok.text)>("in")) {
        throw_parser_error("expected 'in', got '" + tok.describe() + "'");
      }
      get_next_token();

      if (!parse_expression(tmpl, closing)) {
        return false;
      }
    } else if (tok.text == static_cast<decltype(tok.text)>("endfor")) {
      if (for_statement_stack.empty()) {
        throw_parser_error("endfor without matching for");
      }

      auto& for_statement_data = for_statement_stack.top();
      get_next_token();

      current_block = for_statement_data->parent;
      for_statement_stack.pop();
    } else if (tok.text == static_cast<decltype(tok.text)>("include")) {
      get_next_token();

      std::string template_name = parse_filename(tok);
      add_to_template_storage(path, template_name);

      current_block->nodes.emplace_back(std::make_shared<IncludeStatementNode>(template_name, tok.text.data() - tmpl.content.c_str()));

      get_next_token();
    } else if (tok.text == static_cast<decltype(tok.text)>("extends")) {
      get_next_token();

      std::string template_name = parse_filename(tok);
      add_to_template_storage(path, template_name);

      current_block->nodes.emplace_back(std::make_shared<ExtendsStatementNode>(template_name, tok.text.data() - tmpl.content.c_str()));

      get_next_token();
    } else if (tok.text == static_cast<decltype(tok.text)>("set")) {
      get_next_token();

      if (tok.kind != Token::Kind::Id) {
        throw_parser_error("expected variable name, got '" + tok.describe() + "'");
      }

      std::string key = static_cast<std::string>(tok.text);
      get_next_token();

      auto set_statement_node = std::make_shared<SetStatementNode>(key, tok.text.data() - tmpl.content.c_str());
      current_block->nodes.emplace_back(set_statement_node);
      current_expression_list = &set_statement_node->expression;

      if (tok.text != static_cast<decltype(tok.text)>("=")) {
        throw_parser_error("expected '=', got '" + tok.describe() + "'");
      }
      get_next_token();

      if (!parse_expression(tmpl, closing)) {
        return false;
      }
    } else {
      return false;
    }
    return true;
  }

  void parse_into(Template& tmpl, std::string_view path) {
    lexer.start(tmpl.content);
    current_block = &tmpl.root;

    for (;;) {
      get_next_token();
      switch (tok.kind) {
      case Token::Kind::Eof: {
        if (!if_statement_stack.empty()) {
          throw_parser_error("unmatched if");
        }
        if (!for_statement_stack.empty()) {
          throw_parser_error("unmatched for");
        }
      }
        return;
      case Token::Kind::Text: {
        current_block->nodes.emplace_back(std::make_shared<TextNode>(tok.text.data() - tmpl.content.c_str(), tok.text.size()));
      } break;
      case Token::Kind::StatementOpen: {
        get_next_token();
        if (!parse_statement(tmpl, Token::Kind::StatementClose, path)) {
          throw_parser_error("expected statement, got '" + tok.describe() + "'");
        }
        if (tok.kind != Token::Kind::StatementClose) {
          throw_parser_error("expected statement close, got '" + tok.describe() + "'");
        }
      } break;
      case Token::Kind::LineStatementOpen: {
        get_next_token();
        if (!parse_statement(tmpl, Token::Kind::LineStatementClose, path)) {
          throw_parser_error("expected statement, got '" + tok.describe() + "'");
        }
        if (tok.kind != Token::Kind::LineStatementClose && tok.kind != Token::Kind::Eof) {
          throw_parser_error("expected line statement close, got '" + tok.describe() + "'");
        }
      } break;
      case Token::Kind::ExpressionOpen: {
        get_next_token();

        auto expression_list_node = std::make_shared<ExpressionListNode>(tok.text.data() - tmpl.content.c_str());
        current_block->nodes.emplace_back(expression_list_node);
        current_expression_list = expression_list_node.get();

        if (!parse_expression(tmpl, Token::Kind::ExpressionClose)) {
          throw_parser_error("expected expression, got '" + tok.describe() + "'");
        }

        if (tok.kind != Token::Kind::ExpressionClose) {
          throw_parser_error("expected expression close, got '" + tok.describe() + "'");
        }
      } break;
      case Token::Kind::CommentOpen: {
        get_next_token();
        if (tok.kind != Token::Kind::CommentClose) {
          throw_parser_error("expected comment close, got '" + tok.describe() + "'");
        }
      } break;
      default: {
        throw_parser_error("unexpected token '" + tok.describe() + "'");
      } break;
      }
    }
  }

public:
  explicit Parser(const ParserConfig& parser_config, const LexerConfig& lexer_config, TemplateStorage& template_storage,
                  const FunctionStorage& function_storage)
      : config(parser_config), lexer(lexer_config), template_storage(template_storage), function_storage(function_storage) {}

  Template parse(std::string_view input, std::string_view path) {
    auto result = Template(static_cast<std::string>(input));
    parse_into(result, path);
    return result;
  }

  Template parse(std::string_view input) {
    return parse(input, "./");
  }

  void parse_into_template(Template& tmpl, std::string_view filename) {
    std::string_view path = filename.substr(0, filename.find_last_of("/\\") + 1);

    // StringRef path = sys::path::parent_path(filename);
    auto sub_parser = Parser(config, lexer.get_config(), template_storage, function_storage);
    sub_parser.parse_into(tmpl, path);
  }

  std::string load_file(const std::string& filename) {
    std::ifstream file;
    file.open(filename);
    if (file.fail()) {
      INJA_THROW(FileError("failed accessing file at '" + filename + "'"));
    }
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return text;
  }
};

} // namespace inja

#endif // INCLUDE_INJA_PARSER_HPP_

// #include "renderer.hpp"
#ifndef INCLUDE_INJA_RENDERER_HPP_
#define INCLUDE_INJA_RENDERER_HPP_

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

// #include "config.hpp"

// #include "exceptions.hpp"

// #include "node.hpp"

// #include "template.hpp"

// #include "utils.hpp"


namespace inja {

/*!
 * \brief Class for rendering a Template with data.
 */
class Renderer : public NodeVisitor {
  using Op = FunctionStorage::Operation;

  const RenderConfig config;
  const TemplateStorage& template_storage;
  const FunctionStorage& function_storage;

  const Template* current_template;
  size_t current_level {0};
  std::vector<const Template*> template_stack;
  std::vector<const BlockStatementNode*> block_statement_stack;

  const json* data_input;
  std::ostream* output_stream;

  json additional_data;
  json* current_loop_data = &additional_data["loop"];

  std::vector<std::shared_ptr<json>> data_tmp_stack;
  std::stack<const json*> data_eval_stack;
  std::stack<const DataNode*> not_found_stack;

  bool break_rendering {false};

  static bool truthy(const json* data) {
    if (data->is_boolean()) {
      return data->get<bool>();
    } else if (data->is_number()) {
      return (*data != 0);
    } else if (data->is_null()) {
      return false;
    }
    return !data->empty();
  }

  void print_data(const std::shared_ptr<json> value) {
    if (value->is_string()) {
      *output_stream << value->get_ref<const json::string_t&>();
    } else if (value->is_number_integer()) {
      *output_stream << value->get<const json::number_integer_t>();
    } else if (value->is_null()) {
    } else {
      *output_stream << value->dump();
    }
  }

  const std::shared_ptr<json> eval_expression_list(const ExpressionListNode& expression_list) {
    if (!expression_list.root) {
      throw_renderer_error("empty expression", expression_list);
    }

    expression_list.root->accept(*this);

    if (data_eval_stack.empty()) {
      throw_renderer_error("empty expression", expression_list);
    } else if (data_eval_stack.size() != 1) {
      throw_renderer_error("malformed expression", expression_list);
    }

    const auto result = data_eval_stack.top();
    data_eval_stack.pop();

    if (!result) {
      if (not_found_stack.empty()) {
        throw_renderer_error("expression could not be evaluated", expression_list);
      }

      auto node = not_found_stack.top();
      not_found_stack.pop();

      throw_renderer_error("variable '" + static_cast<std::string>(node->name) + "' not found", *node);
    }
    return std::make_shared<json>(*result);
  }

  void throw_renderer_error(const std::string& message, const AstNode& node) {
    SourceLocation loc = get_source_location(current_template->content, node.pos);
    INJA_THROW(RenderError(message, loc));
  }

  void make_result(const json&& result) {
    auto result_ptr = std::make_shared<json>(result);
    data_tmp_stack.push_back(result_ptr);
    data_eval_stack.push(result_ptr.get());
  }

  template <size_t N, size_t N_start = 0, bool throw_not_found = true> std::array<const json*, N> get_arguments(const FunctionNode& node) {
    if (node.arguments.size() < N_start + N) {
      throw_renderer_error("function needs " + std::to_string(N_start + N) + " variables, but has only found " + std::to_string(node.arguments.size()), node);
    }

    for (size_t i = N_start; i < N_start + N; i += 1) {
      node.arguments[i]->accept(*this);
    }

    if (data_eval_stack.size() < N) {
      throw_renderer_error("function needs " + std::to_string(N) + " variables, but has only found " + std::to_string(data_eval_stack.size()), node);
    }

    std::array<const json*, N> result;
    for (size_t i = 0; i < N; i += 1) {
      result[N - i - 1] = data_eval_stack.top();
      data_eval_stack.pop();

      if (!result[N - i - 1]) {
        const auto data_node = not_found_stack.top();
        not_found_stack.pop();

        if (throw_not_found) {
          throw_renderer_error("variable '" + static_cast<std::string>(data_node->name) + "' not found", *data_node);
        }
      }
    }
    return result;
  }

  template <bool throw_not_found = true> Arguments get_argument_vector(const FunctionNode& node) {
    const size_t N = node.arguments.size();
    for (auto a : node.arguments) {
      a->accept(*this);
    }

    if (data_eval_stack.size() < N) {
      throw_renderer_error("function needs " + std::to_string(N) + " variables, but has only found " + std::to_string(data_eval_stack.size()), node);
    }

    Arguments result {N};
    for (size_t i = 0; i < N; i += 1) {
      result[N - i - 1] = data_eval_stack.top();
      data_eval_stack.pop();

      if (!result[N - i - 1]) {
        const auto data_node = not_found_stack.top();
        not_found_stack.pop();

        if (throw_not_found) {
          throw_renderer_error("variable '" + static_cast<std::string>(data_node->name) + "' not found", *data_node);
        }
      }
    }
    return result;
  }

  void visit(const BlockNode& node) {
    for (auto& n : node.nodes) {
      n->accept(*this);

      if (break_rendering) {
        break;
      }
    }
  }

  void visit(const TextNode& node) {
    output_stream->write(current_template->content.c_str() + node.pos, node.length);
  }

  void visit(const ExpressionNode&) {}

  void visit(const LiteralNode& node) {
    data_eval_stack.push(&node.value);
  }

  void visit(const DataNode& node) {
    if (additional_data.contains(node.ptr)) {
      data_eval_stack.push(&(additional_data[node.ptr]));
    } else if (data_input->contains(node.ptr)) {
      data_eval_stack.push(&(*data_input)[node.ptr]);
    } else {
      // Try to evaluate as a no-argument callback
      const auto function_data = function_storage.find_function(node.name, 0);
      if (function_data.operation == FunctionStorage::Operation::Callback) {
        Arguments empty_args {};
        const auto value = std::make_shared<json>(function_data.callback(empty_args));
        data_tmp_stack.push_back(value);
        data_eval_stack.push(value.get());
      } else {
        data_eval_stack.push(nullptr);
        not_found_stack.emplace(&node);
      }
    }
  }

  void visit(const FunctionNode& node) {
    switch (node.operation) {
    case Op::Not: {
      const auto args = get_arguments<1>(node);
      make_result(!truthy(args[0]));
    } break;
    case Op::And: {
      make_result(truthy(get_arguments<1, 0>(node)[0]) && truthy(get_arguments<1, 1>(node)[0]));
    } break;
    case Op::Or: {
      make_result(truthy(get_arguments<1, 0>(node)[0]) || truthy(get_arguments<1, 1>(node)[0]));
    } break;
    case Op::In: {
      const auto args = get_arguments<2>(node);
      make_result(std::find(args[1]->begin(), args[1]->end(), *args[0]) != args[1]->end());
    } break;
    case Op::Equal: {
      const auto args = get_arguments<2>(node);
      make_result(*args[0] == *args[1]);
    } break;
    case Op::NotEqual: {
      const auto args = get_arguments<2>(node);
      make_result(*args[0] != *args[1]);
    } break;
    case Op::Greater: {
      const auto args = get_arguments<2>(node);
      make_result(*args[0] > *args[1]);
    } break;
    case Op::GreaterEqual: {
      const auto args = get_arguments<2>(node);
      make_result(*args[0] >= *args[1]);
    } break;
    case Op::Less: {
      const auto args = get_arguments<2>(node);
      make_result(*args[0] < *args[1]);
    } break;
    case Op::LessEqual: {
      const auto args = get_arguments<2>(node);
      make_result(*args[0] <= *args[1]);
    } break;
    case Op::Add: {
      const auto args = get_arguments<2>(node);
      if (args[0]->is_string() && args[1]->is_string()) {
        make_result(args[0]->get_ref<const std::string&>() + args[1]->get_ref<const std::string&>());
      } else if (args[0]->is_number_integer() && args[1]->is_number_integer()) {
        make_result(args[0]->get<int>() + args[1]->get<int>());
      } else {
        make_result(args[0]->get<double>() + args[1]->get<double>());
      }
    } break;
    case Op::Subtract: {
      const auto args = get_arguments<2>(node);
      if (args[0]->is_number_integer() && args[1]->is_number_integer()) {
        make_result(args[0]->get<int>() - args[1]->get<int>());
      } else {
        make_result(args[0]->get<double>() - args[1]->get<double>());
      }
    } break;
    case Op::Multiplication: {
      const auto args = get_arguments<2>(node);
      if (args[0]->is_number_integer() && args[1]->is_number_integer()) {
        make_result(args[0]->get<int>() * args[1]->get<int>());
      } else {
        make_result(args[0]->get<double>() * args[1]->get<double>());
      }
    } break;
    case Op::Division: {
      const auto args = get_arguments<2>(node);
      if (args[1]->get<double>() == 0) {
        throw_renderer_error("division by zero", node);
      }
      make_result(args[0]->get<double>() / args[1]->get<double>());
    } break;
    case Op::Power: {
      const auto args = get_arguments<2>(node);
      if (args[0]->is_number_integer() && args[1]->get<int>() >= 0) {
        int result = static_cast<int>(std::pow(args[0]->get<int>(), args[1]->get<int>()));
        make_result(result);
      } else {
        double result = std::pow(args[0]->get<double>(), args[1]->get<int>());
        make_result(result);
      }
    } break;
    case Op::Modulo: {
      const auto args = get_arguments<2>(node);
      make_result(args[0]->get<int>() % args[1]->get<int>());
    } break;
    case Op::AtId: {
      const auto container = get_arguments<1, 0, false>(node)[0];
      node.arguments[1]->accept(*this);
      if (not_found_stack.empty()) {
        throw_renderer_error("could not find element with given name", node);
      }
      const auto id_node = not_found_stack.top();
      not_found_stack.pop();
      data_eval_stack.pop();
      data_eval_stack.push(&container->at(id_node->name));
    } break;
    case Op::At: {
      const auto args = get_arguments<2>(node);
      if (args[0]->is_object()) {
        data_eval_stack.push(&args[0]->at(args[1]->get<std::string>()));
      } else {
        data_eval_stack.push(&args[0]->at(args[1]->get<int>()));
      }
    } break;
    case Op::Default: {
      const auto test_arg = get_arguments<1, 0, false>(node)[0];
      data_eval_stack.push(test_arg ? test_arg : get_arguments<1, 1>(node)[0]);
    } break;
    case Op::DivisibleBy: {
      const auto args = get_arguments<2>(node);
      const int divisor = args[1]->get<int>();
      make_result((divisor != 0) && (args[0]->get<int>() % divisor == 0));
    } break;
    case Op::Even: {
      make_result(get_arguments<1>(node)[0]->get<int>() % 2 == 0);
    } break;
    case Op::Exists: {
      auto&& name = get_arguments<1>(node)[0]->get_ref<const std::string&>();
      make_result(data_input->contains(json::json_pointer(DataNode::convert_dot_to_ptr(name))));
    } break;
    case Op::ExistsInObject: {
      const auto args = get_arguments<2>(node);
      auto&& name = args[1]->get_ref<const std::string&>();
      make_result(args[0]->find(name) != args[0]->end());
    } break;
    case Op::First: {
      const auto result = &get_arguments<1>(node)[0]->front();
      data_eval_stack.push(result);
    } break;
    case Op::Float: {
      make_result(std::stod(get_arguments<1>(node)[0]->get_ref<const std::string&>()));
    } break;
    case Op::Int: {
      make_result(std::stoi(get_arguments<1>(node)[0]->get_ref<const std::string&>()));
    } break;
    case Op::Last: {
      const auto result = &get_arguments<1>(node)[0]->back();
      data_eval_stack.push(result);
    } break;
    case Op::Length: {
      const auto val = get_arguments<1>(node)[0];
      if (val->is_string()) {
        make_result(val->get_ref<const std::string&>().length());
      } else {
        make_result(val->size());
      }
    } break;
    case Op::Lower: {
      std::string result = get_arguments<1>(node)[0]->get<std::string>();
      std::transform(result.begin(), result.end(), result.begin(), ::tolower);
      make_result(std::move(result));
    } break;
    case Op::Max: {
      const auto args = get_arguments<1>(node);
      const auto result = std::max_element(args[0]->begin(), args[0]->end());
      data_eval_stack.push(&(*result));
    } break;
    case Op::Min: {
      const auto args = get_arguments<1>(node);
      const auto result = std::min_element(args[0]->begin(), args[0]->end());
      data_eval_stack.push(&(*result));
    } break;
    case Op::Odd: {
      make_result(get_arguments<1>(node)[0]->get<int>() % 2 != 0);
    } break;
    case Op::Range: {
      std::vector<int> result(get_arguments<1>(node)[0]->get<int>());
      std::iota(result.begin(), result.end(), 0);
      make_result(std::move(result));
    } break;
    case Op::Round: {
      const auto args = get_arguments<2>(node);
      const int precision = args[1]->get<int>();
      const double result = std::round(args[0]->get<double>() * std::pow(10.0, precision)) / std::pow(10.0, precision);
      if (precision == 0) {
        make_result(int(result));
      } else {
        make_result(result);
      }
    } break;
    case Op::Sort: {
      auto result_ptr = std::make_shared<json>(get_arguments<1>(node)[0]->get<std::vector<json>>());
      std::sort(result_ptr->begin(), result_ptr->end());
      data_tmp_stack.push_back(result_ptr);
      data_eval_stack.push(result_ptr.get());
    } break;
    case Op::Upper: {
      std::string result = get_arguments<1>(node)[0]->get<std::string>();
      std::transform(result.begin(), result.end(), result.begin(), ::toupper);
      make_result(std::move(result));
    } break;
    case Op::IsBoolean: {
      make_result(get_arguments<1>(node)[0]->is_boolean());
    } break;
    case Op::IsNumber: {
      make_result(get_arguments<1>(node)[0]->is_number());
    } break;
    case Op::IsInteger: {
      make_result(get_arguments<1>(node)[0]->is_number_integer());
    } break;
    case Op::IsFloat: {
      make_result(get_arguments<1>(node)[0]->is_number_float());
    } break;
    case Op::IsObject: {
      make_result(get_arguments<1>(node)[0]->is_object());
    } break;
    case Op::IsArray: {
      make_result(get_arguments<1>(node)[0]->is_array());
    } break;
    case Op::IsString: {
      make_result(get_arguments<1>(node)[0]->is_string());
    } break;
    case Op::Callback: {
      auto args = get_argument_vector(node);
      make_result(node.callback(args));
    } break;
    case Op::Super: {
      const auto args = get_argument_vector(node);
      const size_t old_level = current_level;
      const size_t level_diff = (args.size() == 1) ? args[0]->get<int>() : 1;
      const size_t level = current_level + level_diff;

      if (block_statement_stack.empty()) {
        throw_renderer_error("super() call is not within a block", node);
      }

      if (level < 1 || level > template_stack.size() - 1) {
        throw_renderer_error("level of super() call does not match parent templates (between 1 and " + std::to_string(template_stack.size() - 1) + ")", node);
      }

      const auto current_block_statement = block_statement_stack.back();
      const Template* new_template = template_stack.at(level);
      const Template* old_template = current_template;
      const auto block_it = new_template->block_storage.find(current_block_statement->name);
      if (block_it != new_template->block_storage.end()) {
        current_template = new_template;
        current_level = level;
        block_it->second->block.accept(*this);
        current_level = old_level;
        current_template = old_template;
      } else {
        throw_renderer_error("could not find block with name '" + current_block_statement->name + "'", node);
      }
      make_result(nullptr);
    } break;
    case Op::Join: {
      const auto args = get_arguments<2>(node);
      const auto separator = args[1]->get<std::string>();
      std::ostringstream os;
      std::string sep;
      for (const auto& value : *args[0]) {
        os << sep;
        if (value.is_string()) {
          os << value.get<std::string>(); // otherwise the value is surrounded with ""
        } else {
          os << value.dump();
        }
        sep = separator;
      }
      make_result(os.str());
    } break;
    case Op::ParenLeft:
    case Op::ParenRight:
    case Op::None:
      break;
    }
  }

  void visit(const ExpressionListNode& node) {
    print_data(eval_expression_list(node));
  }

  void visit(const StatementNode&) {}

  void visit(const ForStatementNode&) {}

  void visit(const ForArrayStatementNode& node) {
    const auto result = eval_expression_list(node.condition);
    if (!result->is_array()) {
      throw_renderer_error("object must be an array", node);
    }

    if (!current_loop_data->empty()) {
      auto tmp = *current_loop_data; // Because of clang-3
      (*current_loop_data)["parent"] = std::move(tmp);
    }

    size_t index = 0;
    (*current_loop_data)["is_first"] = true;
    (*current_loop_data)["is_last"] = (result->size() <= 1);
    for (auto it = result->begin(); it != result->end(); ++it) {
      additional_data[static_cast<std::string>(node.value)] = *it;

      (*current_loop_data)["index"] = index;
      (*current_loop_data)["index1"] = index + 1;
      if (index == 1) {
        (*current_loop_data)["is_first"] = false;
      }
      if (index == result->size() - 1) {
        (*current_loop_data)["is_last"] = true;
      }

      node.body.accept(*this);
      ++index;
    }

    additional_data[static_cast<std::string>(node.value)].clear();
    if (!(*current_loop_data)["parent"].empty()) {
      const auto tmp = (*current_loop_data)["parent"];
      *current_loop_data = std::move(tmp);
    } else {
      current_loop_data = &additional_data["loop"];
    }
  }

  void visit(const ForObjectStatementNode& node) {
    const auto result = eval_expression_list(node.condition);
    if (!result->is_object()) {
      throw_renderer_error("object must be an object", node);
    }

    if (!current_loop_data->empty()) {
      (*current_loop_data)["parent"] = std::move(*current_loop_data);
    }

    size_t index = 0;
    (*current_loop_data)["is_first"] = true;
    (*current_loop_data)["is_last"] = (result->size() <= 1);
    for (auto it = result->begin(); it != result->end(); ++it) {
      additional_data[static_cast<std::string>(node.key)] = it.key();
      additional_data[static_cast<std::string>(node.value)] = it.value();

      (*current_loop_data)["index"] = index;
      (*current_loop_data)["index1"] = index + 1;
      if (index == 1) {
        (*current_loop_data)["is_first"] = false;
      }
      if (index == result->size() - 1) {
        (*current_loop_data)["is_last"] = true;
      }

      node.body.accept(*this);
      ++index;
    }

    additional_data[static_cast<std::string>(node.key)].clear();
    additional_data[static_cast<std::string>(node.value)].clear();
    if (!(*current_loop_data)["parent"].empty()) {
      *current_loop_data = std::move((*current_loop_data)["parent"]);
    } else {
      current_loop_data = &additional_data["loop"];
    }
  }

  void visit(const IfStatementNode& node) {
    const auto result = eval_expression_list(node.condition);
    if (truthy(result.get())) {
      node.true_statement.accept(*this);
    } else if (node.has_false_statement) {
      node.false_statement.accept(*this);
    }
  }

  void visit(const IncludeStatementNode& node) {
    auto sub_renderer = Renderer(config, template_storage, function_storage);
    const auto included_template_it = template_storage.find(node.file);
    if (included_template_it != template_storage.end()) {
      sub_renderer.render_to(*output_stream, included_template_it->second, *data_input, &additional_data);
    } else if (config.throw_at_missing_includes) {
      throw_renderer_error("include '" + node.file + "' not found", node);
    }
  }

  void visit(const ExtendsStatementNode& node) {
    const auto included_template_it = template_storage.find(node.file);
    if (included_template_it != template_storage.end()) {
      const Template* parent_template = &included_template_it->second;
      render_to(*output_stream, *parent_template, *data_input, &additional_data);
      break_rendering = true;
    } else if (config.throw_at_missing_includes) {
      throw_renderer_error("extends '" + node.file + "' not found", node);
    }
  }

  void visit(const BlockStatementNode& node) {
    const size_t old_level = current_level;
    current_level = 0;
    current_template = template_stack.front();
    const auto block_it = current_template->block_storage.find(node.name);
    if (block_it != current_template->block_storage.end()) {
      block_statement_stack.emplace_back(&node);
      block_it->second->block.accept(*this);
      block_statement_stack.pop_back();
    }
    current_level = old_level;
    current_template = template_stack.back();
  }

  void visit(const SetStatementNode& node) {
    std::string ptr = node.key;
    replace_substring(ptr, ".", "/");
    ptr = "/" + ptr;
    additional_data[json::json_pointer(ptr)] = *eval_expression_list(node.expression);
  }

public:
  Renderer(const RenderConfig& config, const TemplateStorage& template_storage, const FunctionStorage& function_storage)
      : config(config), template_storage(template_storage), function_storage(function_storage) {}

  void render_to(std::ostream& os, const Template& tmpl, const json& data, json* loop_data = nullptr) {
    output_stream = &os;
    current_template = &tmpl;
    data_input = &data;
    if (loop_data) {
      additional_data = *loop_data;
      current_loop_data = &additional_data["loop"];
    }

    template_stack.emplace_back(current_template);
    current_template->root.accept(*this);

    data_tmp_stack.clear();
  }
};

} // namespace inja

#endif // INCLUDE_INJA_RENDERER_HPP_

// #include "template.hpp"

// #include "utils.hpp"


namespace inja {

/*!
 * \brief Class for changing the configuration.
 */
class Environment {
  std::string input_path;
  std::string output_path;

  LexerConfig lexer_config;
  ParserConfig parser_config;
  RenderConfig render_config;

  FunctionStorage function_storage;
  TemplateStorage template_storage;

public:
  Environment(): Environment("") {}

  explicit Environment(const std::string& global_path): input_path(global_path), output_path(global_path) {}

  Environment(const std::string& input_path, const std::string& output_path): input_path(input_path), output_path(output_path) {}

  /// Sets the opener and closer for template statements
  void set_statement(const std::string& open, const std::string& close) {
    lexer_config.statement_open = open;
    lexer_config.statement_open_no_lstrip = open + "+";
    lexer_config.statement_open_force_lstrip = open + "-";
    lexer_config.statement_close = close;
    lexer_config.statement_close_force_rstrip = "-" + close;
    lexer_config.update_open_chars();
  }

  /// Sets the opener for template line statements
  void set_line_statement(const std::string& open) {
    lexer_config.line_statement = open;
    lexer_config.update_open_chars();
  }

  /// Sets the opener and closer for template expressions
  void set_expression(const std::string& open, const std::string& close) {
    lexer_config.expression_open = open;
    lexer_config.expression_open_force_lstrip = open + "-";
    lexer_config.expression_close = close;
    lexer_config.expression_close_force_rstrip = "-" + close;
    lexer_config.update_open_chars();
  }

  /// Sets the opener and closer for template comments
  void set_comment(const std::string& open, const std::string& close) {
    lexer_config.comment_open = open;
    lexer_config.comment_open_force_lstrip = open + "-";
    lexer_config.comment_close = close;
    lexer_config.comment_close_force_rstrip = "-" + close;
    lexer_config.update_open_chars();
  }

  /// Sets whether to remove the first newline after a block
  void set_trim_blocks(bool trim_blocks) {
    lexer_config.trim_blocks = trim_blocks;
  }

  /// Sets whether to strip the spaces and tabs from the start of a line to a block
  void set_lstrip_blocks(bool lstrip_blocks) {
    lexer_config.lstrip_blocks = lstrip_blocks;
  }

  /// Sets the element notation syntax
  void set_search_included_templates_in_files(bool search_in_files) {
    parser_config.search_included_templates_in_files = search_in_files;
  }

  /// Sets whether a missing include will throw an error
  void set_throw_at_missing_includes(bool will_throw) {
    render_config.throw_at_missing_includes = will_throw;
  }

  Template parse(std::string_view input) {
    Parser parser(parser_config, lexer_config, template_storage, function_storage);
    return parser.parse(input);
  }

  Template parse_template(const std::string& filename) {
    Parser parser(parser_config, lexer_config, template_storage, function_storage);
    auto result = Template(parser.load_file(input_path + static_cast<std::string>(filename)));
    parser.parse_into_template(result, input_path + static_cast<std::string>(filename));
    return result;
  }

  Template parse_file(const std::string& filename) {
    return parse_template(filename);
  }

  std::string render(std::string_view input, const json& data) {
    return render(parse(input), data);
  }

  std::string render(const Template& tmpl, const json& data) {
    std::stringstream os;
    render_to(os, tmpl, data);
    return os.str();
  }

  std::string render_file(const std::string& filename, const json& data) {
    return render(parse_template(filename), data);
  }

  std::string render_file_with_json_file(const std::string& filename, const std::string& filename_data) {
    const json data = load_json(filename_data);
    return render_file(filename, data);
  }

  void write(const std::string& filename, const json& data, const std::string& filename_out) {
    std::ofstream file(output_path + filename_out);
    file << render_file(filename, data);
    file.close();
  }

  void write(const Template& temp, const json& data, const std::string& filename_out) {
    std::ofstream file(output_path + filename_out);
    file << render(temp, data);
    file.close();
  }

  void write_with_json_file(const std::string& filename, const std::string& filename_data, const std::string& filename_out) {
    const json data = load_json(filename_data);
    write(filename, data, filename_out);
  }

  void write_with_json_file(const Template& temp, const std::string& filename_data, const std::string& filename_out) {
    const json data = load_json(filename_data);
    write(temp, data, filename_out);
  }

  std::ostream& render_to(std::ostream& os, const Template& tmpl, const json& data) {
    Renderer(render_config, template_storage, function_storage).render_to(os, tmpl, data);
    return os;
  }

  std::string load_file(const std::string& filename) {
    Parser parser(parser_config, lexer_config, template_storage, function_storage);
    return parser.load_file(input_path + filename);
  }

  json load_json(const std::string& filename) {
    std::ifstream file;
    file.open(input_path + filename);
    if (file.fail()) {
      INJA_THROW(FileError("failed accessing file at '" + input_path + filename + "'"));
    }

    return json::parse(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  }

  /*!
  @brief Adds a variadic callback
  */
  void add_callback(const std::string& name, const CallbackFunction& callback) {
    add_callback(name, -1, callback);
  }

  /*!
  @brief Adds a variadic void callback
  */
  void add_void_callback(const std::string& name, const VoidCallbackFunction& callback) {
    add_void_callback(name, -1, callback);
  }

  /*!
  @brief Adds a callback with given number or arguments
  */
  void add_callback(const std::string& name, int num_args, const CallbackFunction& callback) {
    function_storage.add_callback(name, num_args, callback);
  }

  /*!
  @brief Adds a void callback with given number or arguments
  */
  void add_void_callback(const std::string& name, int num_args, const VoidCallbackFunction& callback) {
    function_storage.add_callback(name, num_args, [callback](Arguments& args) {
      callback(args);
      return json();
    });
  }

  /** Includes a template with a given name into the environment.
   * Then, a template can be rendered in another template using the
   * include "<name>" syntax.
   */
  void include_template(const std::string& name, const Template& tmpl) {
    template_storage[name] = tmpl;
  }

  /*!
  @brief Sets a function that is called when an included file is not found
  */
  void set_include_callback(const std::function<Template(const std::string&, const std::string&)>& callback) {
    parser_config.include_callback = callback;
  }
};

/*!
@brief render with default settings to a string
*/
inline std::string render(std::string_view input, const json& data) {
  return Environment().render(input, data);
}

/*!
@brief render with default settings to the given output stream
*/
inline void render_to(std::ostream& os, std::string_view input, const json& data) {
  Environment env;
  env.render_to(os, env.parse(input), data);
}

} // namespace inja

#endif // INCLUDE_INJA_ENVIRONMENT_HPP_

// #include "exceptions.hpp"

// #include "parser.hpp"

// #include "renderer.hpp"

// #include "template.hpp"


#endif // INCLUDE_INJA_INJA_HPP_
