ZeroTier 官方发布步骤
======

这主要是供 ZeroTier 内部使用，但其他想要构建的人可能会发现它很有帮助。

注意：许多这些步骤将需要保存在冷存储中并必须挂载的 GPG 和其他签名密钥。

# 提升版本和准备安装程序

必须在以下所有文件中递增版本：

    /version.h
    /zerotier-one.spec
    /debian/changelog
    /ext/installfiles/mac/ZeroTier One.pkgproj
    /ext/installfiles/windows/ZeroTier One.aip
  ../DesktopUI/mac-app-template/ZeroTier.app/Contents/Info.plist

最终的 .AIP 文件只能在 Windows 上使用 [Advanced Installer Enterprise](http://www.advancedinstaller.com/) 编辑。除了递增版本外，请确保生成了新的产品代码。（另一方面，"升级代码" GUID 绝对不能更改。）

# 为支持的平台构建

## Macintosh

Mac 很简单。只需输入：

    make official

您将需要 [Packages](http://s.sudre.free.fr/Software/Packages/about.html) 和我们发布签名密钥在钥匙串中。

## Windows

首先加载 Visual Studio 解决方案，并在 x64、i386 和 arm64 `Release` 模式下重新构建 UI 和 ZeroTier One。然后加载 [Advanced Installer Enterprise](http://www.advancedinstaller.com/)，检查版本是否正确，然后构建。如果缺少任何构建工件，构建将失败，并且 Windows 必须有我们的产品签名密钥（来自 DigiCert）来签署生成的 MSI 文件。然后必须在至少几个不同的干净 Windows 虚拟机上测试 MSI，以确保安装程序有效且正确签名。