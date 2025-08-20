# Using clang-format

## Command-line use

To format a file according to Coding style using command line, please setup the
build tools environment first.

For Windows - cmd.exe command:

```call tools\setup-devenv.cmd```

For Linux and Mac:

```. tools/setup-devenv.sh```

Command will add the tools from repo *tools* directory to PATH environment
variable.

Then run:

```git cl format <filename>```

At the moment the tool requires you to specify the path to file you want to
format. Long-term goal is to integrate the [clang-format from
Chromium](https://chromium.googlesource.com/chromium/src/+/master/docs/clang_format.md)
to automatically format all source code files being changed.

## Alternative

Run: `tools/format.sh`

Or, via Docker: `./ci/run_docker.sh ./ci/do_ci.sh format`

## Editor integrations

For further guidance on editor integration, see these specific pages:

* [Download link for LLVM tools for
  Windows](https://releases.llvm.org/9.0.0/LLVM-9.0.0-win64.exe)
* [LLVM tools extension for Visual
  Studio](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.llvm-toolchain)
* [Visual Studio code
  extension](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format)
* [CppStyle Eclipse CDT
  extension](https://marketplace.eclipse.org/content/cppstyle)

## Are robots taking over my freedom to choose where newlines go

No. For the project as a whole, using clang-format is just one optional way to
format your code. While it will produce style-guide conformant code, other
formats would also satisfy the style guide. For certain modules it may be
appropriate to use alternate coding style. In those scenarios a local directory
*.clang-format* settings file takes precedence over the one at top-level.
