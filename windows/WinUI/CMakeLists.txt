cmake_minimum_required(VERSION 3.8)
include(CSharpUtilities)

project("ZeroTierOneUI" VERSION 1.4.0 LANGUAGES CSharp)

add_executable(
    ${PROJECT_NAME}
    app.manifest
    App.config
    App.xaml
    App.xaml.cs

    AboutView.xaml
    AboutView.xaml.cs
    APIHandler.cs
    CentralAPI.cs
    CentralLogin.cs
    CentralNetwork.cs
    CentralServer.cs
    CentralToken.cs
    CentralUser.cs
    ISwitchable.cs
    JoinNetworkView.xaml
    JoinNetworkView.xaml.cs
    NetworkInfoView.xaml
    NetworkInfoView.xaml.cs
    NetworkListView.xaml
    NetworkListView.xaml.cs
    NetworkMonitor.cs
    NetworkNameGenerator.cs
    NetworkRoute.cs
    NetworksPage.xaml
    NetworksPage.xaml.cs
    PeersPage.xaml
    PeersPage.xaml.cs
    PreferencesView.xaml
    PreferencesView.xaml.cs
    "Simple Styles.xaml"
    ToolbarItem.xaml
    ToolbarItem.xaml.cs
    ZeroTierNetwork.cs
    ZeroTierPeer.cs
    ZeroTierPeerPhysicalPath.cs
    ZeroTierStatus.cs

    packages.config

    "Properties/AssemblyInfo.cs"
    "Properties/Resources.Designer.cs"
    "Properties/Resources.resx"
    "Properties/Settings.Designer.cs"
    "Properties/Settings.settings"

    "Resources/ZeroTierIcon.ico"
    ZeroTierIcon.ico
)

csharp_set_designer_cs_properties(
    "Properties/AssemblyInfo.cs"
    "Properties/Resources.Designer.cs"
    "Properties/Resources.resx"
    "Properties/Settings.Designer.cs"
    "Properties/Settings.settings"
)

csharp_set_xaml_cs_properties(
    App.xaml
    App.xaml.cs
    AboutView.xaml
    AboutView.xaml.cs
    JoinNetworkView.xaml
    JoinNetworkView.xaml.cs
    NetworkInfoView.xaml
    NetworkInfoView.xaml.cs
    NetworkListView.xaml
    NetworkListView.xaml.cs
    NetworksPage.xaml
    NetworksPage.xaml.cs
    PeersPage.xaml
    PeersPage.xaml.cs
    PreferencesView.xaml
    PreferencesView.xaml.cs
    ToolbarItem.xaml
    ToolbarItem.xaml.cs
)

set_property(SOURCE App.xaml PROPERTY VS_XAML_TYPE "ApplicationDefinition")

set_property(SOURCE ZeroTierIcon.ico PROPERTY VS_TOOL_OVERRIDE "Resource")

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_ROOTNAMESPACE "WinUI"
    VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.5.2"
    WIN32_EXECUTABLE TRUE
    )

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DOTNET_REFERENCES
	"Microsoft.CSharp"
	"PresentationCore"
	"PresentationFramework"
	"System"
	"System.Core"
	"System.Data"
    "System.Data.DataSetExtensions"
    "System.Drawing"
	"System.Net.Http"
	"System.Xaml"
	"System.Xml"
	"System.Xml.Linq"
    "WindowsBase"
    "Newtonsoft.Json"
)

set(CMAKE_CSharp_FLAGS "/langversion:6")

target_compile_options(${PROJECT_NAME} PRIVATE "/win32icon:${CMAKE_CURRENT_SOURCE_DIR}/ZeroTierIcon.ico")
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DOTNET_REFERENCE_Hardcodet.Wpf.TaskbarNotification "${CMAKE_CURRENT_BINARY_DIR}/packages/Hardcodet.NotifyIcon.Wpf.1.0.8/lib/net45/Hardcodet.Wpf.TaskbarNotification.dll")
    
find_program(NUGET nuget)
add_custom_target(nuget-restore
    COMMAND ${NUGET} restore ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.sln
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packages.config
    ${CMAKE_CURRENT_BINARY_DIR}/packages.config COPYONLY)

add_dependencies(${PROJECT_NAME} nuget-restore)
    