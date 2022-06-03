[<div align="center"><img width="500" src="https://raw.githubusercontent.com/pantor/inja/master/doc/logo.svg?sanitize=true"></div>](https://github.com/pantor/inja/releases)

<p align="center">
  <a href="https://github.com/pantor/inja/actions">
    <img src="https://github.com/pantor/inja/workflows/CI/badge.svg" alt="CI Status">
  </a>

  <a href="https://github.com/pantor/inja/actions">
    <img src="https://github.com/pantor/inja/workflows/Documentation/badge.svg" alt="Documentation Status">
  </a>

  <a href="https://www.codacy.com/manual/pantor/inja?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=pantor/inja&amp;utm_campaign=Badge_Grade">
    <img src="https://app.codacy.com/project/badge/Grade/211718f7a36541819d1244c0e2ee6f08"/>
  </a>

  <a href="https://github.com/pantor/inja/releases">
    <img src="https://img.shields.io/github/release/pantor/inja.svg" alt="Github Releases">
  </a>

  <a href="http://github.com/pantor/inja/issues">
    <img src="https://img.shields.io/github/issues/pantor/inja.svg" alt="Github Issues">
  </a>

  <a href="https://raw.githubusercontent.com/pantor/inja/master/LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="GitHub License">
  </a>
</p>

Inja is a template engine for modern C++, loosely inspired by [jinja](http://jinja.pocoo.org) for python. It has an easy and yet powerful template syntax with all variables, loops, conditions, includes, callbacks, and comments you need, nested and combined as you like. Inja uses the wonderful [json](https://github.com/nlohmann/json) library by nlohmann for data input. Most importantly, inja needs only two header files, which is (nearly) as trivial as integration in C++ can get. Of course, everything is tested on all relevant compilers. Here is what it looks like:

```.cpp
json data;
data["name"] = "world";

inja::render("Hello {{ name }}!", data); // Returns "Hello world!"
```

## Integration

Inja is a headers only library, which can be downloaded from the [releases](https://github.com/pantor/inja/releases) or directly from the `include/` or `single_include/` folder. Inja uses `nlohmann/json.hpp` (>= v3.8.0) as its single dependency, so make sure it can be included from `inja.hpp`. json can be downloaded [here](https://github.com/nlohmann/json/releases). Then integration is as easy as:

```.cpp
#include <inja.hpp>

// Just for convenience
using namespace inja;
```

If you are using the [Meson Build System](http://mesonbuild.com), then you can wrap this repository as a subproject.

If you are using [Conan](https://conan.io) to manage your dependencies, have a look at [this repository](https://github.com/DEGoodmanWilson/conan-inja). Please file issues [here](https://github.com/DEGoodmanWilson/conan-inja/issues) if you experience problems with the packages.

You can also integrate inja in your project using [Hunter](https://github.com/cpp-pm/hunter), a package manager for C++.

If you are using [vcpkg](https://github.com/Microsoft/vcpkg) on your project for external dependencies, then you can use the [inja package](https://github.com/Microsoft/vcpkg/tree/master/ports/inja). Please see the vcpkg project for any issues regarding the packaging.

If you are using [cget](https://cget.readthedocs.io/en/latest/), you can install the latest development version with `cget install pantor/inja`. A specific version can be installed with `cget install pantor/inja@v2.1.0`.

On macOS, you can install inja via [Homebrew](https://formulae.brew.sh/formula/inja#default) and `brew install inja`.

If you are using [conda](https://docs.conda.io/en/latest/), you can install the latest version from [conda-forge](https://anaconda.org/conda-forge/inja) with `conda install -c conda-forge inja`.

## Tutorial

This tutorial will give you an idea how to use inja. It will explain the most important concepts and give practical advices using examples and executable code. Beside this tutorial, you may check out the [documentation](https://pantor.github.io/inja).

### Template Rendering

The basic template rendering takes a template as a `std::string` and a `json` object for all data. It returns the rendered template as an `std::string`.

```.cpp
json data;
data["name"] = "world";

render("Hello {{ name }}!", data); // Returns std::string "Hello world!"
render_to(std::cout, "Hello {{ name }}!", data); // Writes "Hello world!" to stream
```

For more advanced usage, an environment is recommended.
```.cpp
Environment env;

// Render a string with json data
std::string result = env.render("Hello {{ name }}!", data); // "Hello world!"

// Or directly read a template file
Template temp = env.parse_template("./templates/greeting.txt");
std::string result = env.render(temp, data); // "Hello world!"

data["name"] = "Inja";
std::string result = env.render(temp, data); // "Hello Inja!"

// Or read the template file (and/or the json file) directly from the environment
result = env.render_file("./templates/greeting.txt", data);
result = env.render_file_with_json_file("./templates/greeting.txt", "./data.json");

// Or write a rendered template file
env.write(temp, data, "./result.txt");
env.write_with_json_file("./templates/greeting.txt", "./data.json", "./result.txt");
```

The environment class can be configured to your needs.
```.cpp
// With default settings
Environment env_default;

// With global path to template files and where files will be saved
Environment env_1 {"../path/templates/"};

// With separate input and output path
Environment env_2 {"../path/templates/", "../path/results/"};

// With other opening and closing strings (here the defaults)
env.set_expression("{{", "}}"); // Expressions
env.set_comment("{#", "#}"); // Comments
env.set_statement("{%", "%}"); // Statements {% %} for many things, see below
env.set_line_statement("##"); // Line statements ## (just an opener)
```

### Variables

Variables are rendered within the `{{ ... }}` expressions.
```.cpp
json data;
data["neighbour"] = "Peter";
data["guests"] = {"Jeff", "Tom", "Patrick"};
data["time"]["start"] = 16;
data["time"]["end"] = 22;

// Indexing in array
render("{{ guests.1 }}", data); // "Tom"

// Objects
render("{{ time.start }} to {{ time.end + 1 }}pm", data); // "16 to 23pm"
```
If no variable is found, valid JSON is printed directly, otherwise an `inja::RenderError` is thrown.

### Statements

Statements can be written either with the `{% ... %}` syntax or the `##` syntax for entire lines. Note that `##` needs to start the line without indentation. The most important statements are loops, conditions and file includes. All statements can be nested.

#### Loops

```.cpp
// Combining loops and line statements
render(R"(Guest List:
## for guest in guests
	{{ loop.index1 }}: {{ guest }}
## endfor )", data)

/* Guest List:
	1: Jeff
	2: Tom
	3: Patrick */
```
In a loop, the special variables `loop.index (number)`, `loop.index1 (number)`, `loop.is_first (boolean)` and `loop.is_last (boolean)` are defined. In nested loops, the parent loop variables are available e.g. via `loop.parent.index`. You can also iterate over objects like `{% for key, value in time %}`.

#### Conditions

Conditions support the typical if, else if and else statements. Following conditions are for example possible:
```.cpp
// Standard comparisons with a variable
render("{% if time.hour >= 20 %}Serve{% else if time.hour >= 18 %}Make{% endif %} dinner.", data); // Serve dinner.

// Variable in list
render("{% if neighbour in guests %}Turn up the music!{% endif %}", data); // Turn up the music!

// Logical operations
render("{% if guest_count < (3+2) and all_tired %}Sleepy...{% else %}Keep going...{% endif %}", data); // Sleepy...

// Negations
render("{% if not guest_count %}The End{% endif %}", data); // The End
```

#### Includes

You can either include other in-memory templates or from the file system.
```.cpp
// To include in-memory templates, add them to the environment first
inja::Template content_template = env.parse("Hello {{ neighbour }}!");
env.include_template("content", content_template);
env.render("Content: {% include \"content\" %}", data); // "Content: Hello Peter!"

// Other template files are included relative from the current file location
render("{% include \"footer.html\" %}", data);
```
If a corresponding template could not be found in the file system, the *include callback* is called:
```.cpp
// The callback takes the current path and the wanted include name and returns a template
env.set_include_callback([&env](const std::string& path, const std::string& template_name) {
  return env.parse("Hello {{ neighbour }} from " + template_name);
});

// You can disable to search for templates in the file system via
env.set_search_included_templates_in_files(false);
```

Inja will throw an `inja::RenderError` if an included file is not found and no callback is specified. To disable this error, you can call `env.set_throw_at_missing_includes(false)`.

#### Assignments

Variables can also be defined within the template using the set statment.
```.cpp
render("{% set new_hour=23 %}{{ new_hour }}pm", data); // "23pm"
render("{% set time.start=18 %}{{ time.start }}pm", data); // using json pointers
```

Assignments only set the value within the rendering context; they do not modify the json object passed into the `render` call.

### Functions

A few functions are implemented within the inja template syntax. They can be called with
```.cpp
// Upper and lower function, for string cases
render("Hello {{ upper(neighbour) }}!", data); // "Hello PETER!"
render("Hello {{ lower(neighbour) }}!", data); // "Hello peter!"

// Range function, useful for loops
render("{% for i in range(4) %}{{ loop.index1 }}{% endfor %}", data); // "1234"
render("{% for i in range(3) %}{{ at(guests, i) }} {% endfor %}", data); // "Jeff Tom Patrick "

// Length function (please don't combine with range, use list directly...)
render("I count {{ length(guests) }} guests.", data); // "I count 3 guests."

// Get first and last element in a list
render("{{ first(guests) }} was first.", data); // "Jeff was first."
render("{{ last(guests) }} was last.", data); // "Patir was last."

// Sort a list
render("{{ sort([3,2,1]) }}", data); // "[1,2,3]"
render("{{ sort(guests) }}", data); // "[\"Jeff\", \"Patrick\", \"Tom\"]"

// Join a list with a separator
render("{{ join([1,2,3], \" + \") }}", data); // "1 + 2 + 3"
render("{{ join(guests, \", \") }}", data); // "Jeff, Patrick, Tom"

// Round numbers to a given precision
render("{{ round(3.1415, 0) }}", data); // 3
render("{{ round(3.1415, 3) }}", data); // 3.142

// Check if a value is odd, even or divisible by a number
render("{{ odd(42) }}", data); // false
render("{{ even(42) }}", data); // true
render("{{ divisibleBy(42, 7) }}", data); // true

// Maximum and minimum values from a list
render("{{ max([1, 2, 3]) }}", data); // 3
render("{{ min([-2.4, -1.2, 4.5]) }}", data); // -2.4

// Convert strings to numbers
render("{{ int(\"2\") == 2 }}", data); // true
render("{{ float(\"1.8\") > 2 }}", data); // false

// Set default values if variables are not defined
render("Hello {{ default(neighbour, \"my friend\") }}!", data); // "Hello Peter!"
render("Hello {{ default(colleague, \"my friend\") }}!", data); // "Hello my friend!"

// Access an objects value dynamically
render("{{ at(time, \"start\") }} to {{ time.end }}", data); // "16 to 22"

// Check if a key exists in an object
render("{{ exists(\"guests\") }}", data); // "true"
render("{{ exists(\"city\") }}", data); // "false"
render("{{ existsIn(time, \"start\") }}", data); // "true"
render("{{ existsIn(time, neighbour) }}", data); // "false"

// Check if a key is a specific type
render("{{ isString(neighbour) }}", data); // "true"
render("{{ isArray(guests) }}", data); // "true"
// Implemented type checks: isArray, isBoolean, isFloat, isInteger, isNumber, isObject, isString,
```

### Callbacks

You can create your own and more complex functions with callbacks. These are implemented with `std::function`, so you can for example use C++ lambdas. Inja `Arguments` are a vector of json pointers.
```.cpp
Environment env;

/*
 * Callbacks are defined by its:
 * - name,
 * - (optional) number of arguments,
 * - callback function.
 */
env.add_callback("double", 1, [](Arguments& args) {
	int number = args.at(0)->get<int>(); // Adapt the index and type of the argument
	return 2 * number;
});

// You can then use a callback like a regular function
env.render("{{ double(16) }}", data); // "32"

// Inja falls back to variadic callbacks if the number of expected arguments is omitted.
env.add_callback("argmax", [](Arguments& args) {
  auto result = std::max_element(args.begin(), args.end(), [](const json* a, const json* b) { return *a < *b;});
  return std::distance(args.begin(), result);
});
env.render("{{ argmax(4, 2, 6) }}", data); // "2"
env.render("{{ argmax(0, 2, 6, 8, 3) }}", data); // "3"

// A callback without argument can be used like a dynamic variable:
std::string greet = "Hello";
env.add_callback("double-greetings", 0, [greet](Arguments args) {
	return greet + " " + greet + "!";
});
env.render("{{ double-greetings }}", data); // "Hello Hello!"
```
You can also add a void callback without return variable, e.g. for debugging:
```.cpp
env.add_void_callback("log", 1, [greet](Arguments args) {
	std::cout << "logging: " << args[0] << std::endl;
});
env.render("{{ log(neighbour) }}", data); // Prints nothing to result, only to cout...
```

### Template Inheritance

Template inheritance allows you to build a base *skeleton* template that contains all the common elements and defines blocks that child templates can override. Lets show an example: The base template
```.html
<!DOCTYPE html>
<html>
<head>
  {% block head %}
  <link rel="stylesheet" href="style.css" />
  <title>{% block title %}{% endblock %} - My Webpage</title>
  {% endblock %}
</head>
<body>
  <div id="content">{% block content %}{% endblock %}</div>
</body>
</html>
```
contains three `blocks` that child templates can fill in. The child template
```.html
{% extends "base.html" %}
{% block title %}Index{% endblock %}
{% block head %}
  {{ super() }}
  <style type="text/css">
    .important { color: #336699; }
  </style>
{% endblock %}
{% block content %}
  <h1>Index</h1>
  <p class="important">
    Welcome to my blog!
  </p>
{% endblock %}
```
calls a parent template with the `extends` keyword; it should be the first element in the template. It is possible to render the contents of the parent block by calling `super()`. In the case of multiple levels of `{% extends %}`, super references may be called with an argument (e.g. `super(2)`) to skip levels in the inheritance tree.

### Whitespace Control

In the default configuration, no whitespace is removed while rendering the file. To support a more readable template style, you can configure the environment to control whitespaces before and after a statement automatically. While enabling `set_trim_blocks` removes the first newline after a statement, `set_lstrip_blocks` strips tabs and spaces from the beginning of a line to the start of a block.

```.cpp
Environment env;
env.set_trim_blocks(true);
env.set_lstrip_blocks(true);
```

With both `trim_blocks` and `lstrip_blocks` enabled, you can put statements on their own lines. Furthermore, you can also strip whitespaces for both statements and expressions by hand. If you add a minus sign (`-`) to the start or end, the whitespaces before or after that block will be removed:

```.cpp
render("Hello       {{- name -}}     !", data); // "Hello Inja!"
render("{% if neighbour in guests -%}   I was there{% endif -%}   !", data); // Renders without any whitespaces
```

Stripping behind a statement or expression also removes any newlines.

### Comments

Comments can be written with the `{# ... #}` syntax.
```.cpp
render("Hello{# Todo #}!", data); // "Hello!"
```

### Exceptions

Inja uses exceptions to handle ill-formed template input. However, exceptions can be switched off with either using the compiler flag `-fno-exceptions` or by defining the symbol `INJA_NOEXCEPTION`. In this case, exceptions are replaced by `abort()` calls.


## Supported compilers

Inja uses the `string_view` feature of the C++17 STL. Currently, the following compilers are tested:

- GCC 7 - 11 (and possibly later)
- Clang 5 - 12 (and possibly later)
- Microsoft Visual C++ 2017 15.0 - 2022 (and possibly later)

A list of supported compiler / os versions can be found in the [CI definition](https://github.com/pantor/inja/blob/master/.github/workflows/ci.yml).
