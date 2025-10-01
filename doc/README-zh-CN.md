手册页和其他文档
=====

使用“./build.sh”构建手册页。

您需要安装 Node.js/npm（脚本会自动安装 npm *marked-man* 包）或 */usr/bin/ronn*。后者是一个 Ruby 程序，在某些发行版中打包为 *rubygem-ronn* 或 *ruby-ronn*，或者可以使用 *gem install ronn* 安装。Node 的 *marked-man* 包和 RubyGems 的 *ronn* 是将 Markdown 编译为 roff/man 格式的两个大致相同的替代方案。