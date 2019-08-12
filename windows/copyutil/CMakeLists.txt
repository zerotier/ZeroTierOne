cmake_minimum_required(VERSION 3.8)
include(CSharpUtilities)

project(copyutil VERSION 1.4.0 LANGUAGES CSharp)

add_executable(
    ${PROJECT_NAME}
    App.config
    Program.cs

    "Properties/AssemblyInfo.cs"
)