[English](README.md) | 简体中文

# README - Fast Light Tool Kit (FLTK) 1.5.0 版

## FLTK 是什么？

Fast Light Tool Kit（FLTK）是一个跨平台的 C++ 图形界面工具包，支持 UNIX®/Linux®（X11 或 Wayland）、Microsoft® Windows® 和 macOS®。FLTK 提供现代图形界面功能，同时保持轻量，并通过 OpenGL® 及其内置的 GLUT 模拟支持三维图形。它最初由 Bill Spitzak 开发，目前由分布在世界各地的一小群开发者维护，中央仓库在 GitHub 上。

  https://www.fltk.org/
  https://github.com/fltk/fltk/

## 许可证

FLTK 提供完整的自由源代码。FLTK 以 GNU Library General Public License 发布，并带有例外条款（例如允许静态链接）。和常见的印象不同，它可以用于商业软件。（就算比尔·盖茨也能用。）

## 在线文档

HTML 和 PDF 形式的文档可以用 Doxygen 从源文件生成。FLTK 网站也提供这两种格式的文档：

  https://www.fltk.org/documentation.php

## 构建 FLTK 的前提

构建 FLTK 1.5 及更高版本需要：

  - CMake
  - 支持 C++11 的编译器，例如 gcc、clang、Visual Studio、Xcode
  - 各系统对应的构建文件（头文件、SDK 等）

CMake 用来在你的系统上生成构建环境。它可以生成多种构建工具的环境，详情见 CMake 文档。

从 FLTK 1.5 起，项目使用 C++11 特性，因此至少需要一台以 C++11 模式运行、支持 C++11 的编译器。以后的版本可能会提高构建 FLTK 和应用程序所用的最低 C++ 标准。

所需的头文件等（构建环境或 SDK）因平台而异，写在各平台的 README.* 文件里。例如在 Unix/Linux 上，这些文件通常由包管理器提供。

## 用 CMake 构建并安装 FLTK（命令行）

在提供命令行 `CMake` 的系统上（Windows 也有），用 CMake 构建 FLTK 可以简单到：

    cd /path/to/fltk
    cmake . -B build [ options ]
    cmake --build build

这几条命令会在源码树里创建 `build` 目录，并构建库和全部测试程序。

测试通过后，可以用下面的命令或类似命令安装库。请注意：如果不修改默认安装路径，这会把 FLTK 安装到系统目录，供全系统使用。除非你清楚自己在做什么，否则不建议这样做。

    sudo cmake --install build  # 在源码树中执行（使用 CMake）

或

    sudo make install           # 在 build 目录中执行（使用 make）

第一条 cmake 命令之后，也可以使用其他命令（例如 `make` 或 `ninja`），取决于你选用的 CMake 生成器。

用 CMake 构建 FLTK 的更多说明见 README.CMake.txt。

## 构建 HTML 文档

FLTK 使用 Doxygen 生成文档，因此至少需要安装 doxygen 才能生成 HTML 文档；要生成 PDF 文档，还需要 LaTeX。

如果要构建文档，请进入构建目录，例如：

    cd /path/to/fltk/build

用下面的 CMake 命令生成 HTML 或 PDF 文档：

    cmake --build . --target html
    cmake --build . --target pdf

说明：除了上面的通用 CMake 命令，也可以使用构建系统自己的等价命令，例如 `make html` 或 `ninja pdf`。

说明：关于构建 Fluid 文档的内容以后补充。

## 网络资源

FLTK 在网上有这些地址：

  - https://www.fltk.org/                   - 主页
  - https://github.com/fltk/fltk            - 源代码和讨论
  - https://www.fltk.org/bugs.php           - 报告缺陷的说明
  - https://www.fltk.org/software.php       - 下载源代码
  - https://github.com/fltk/fltk/releases   - 源代码和文档

我们不提供预编译的二进制发行版。请查看你所用操作系统（Linux、Unix、macOS）的包管理器。

## 一般问题

要加入 FLTK 邮件列表，请打开：

  https://groups.google.com/forum/#!forum/fltkgeneral

即使没有 Google 账号，也可以在下面这个页面底部找到注册邮件列表的详细步骤：

  https://www.fltk.org/newsgroups.php

从 2024 年 7 月起，GitHub 项目页面提供 GitHub Discussions。构建和使用 FLTK 的一般问题请使用 “Q&A” 分区：

  https://github.com/fltk/fltk/discussions/categories/q-a

## 报告缺陷

如果你刚开始使用 FLTK，或者想问如何使用 FLTK，或者还不确定自己发现的是不是缺陷，请先到 fltk.general 论坛提问：

  https://groups.google.com/forum/#!forum/fltkgeneral

也可以使用上面提到的 GitHub Discussions（Q&A）：

  https://github.com/fltk/fltk/discussions/categories/q-a

更多说明见上面的「一般问题」。

如果你确定发现了缺陷，请查看下面的页面，了解如何报告：

  https://www.fltk.org/bugs.php

## 商标

- Microsoft 和 Windows 是 Microsoft Corporation 的注册商标
- UNIX 是 X/Open Group, Inc. 的注册商标
- OpenGL 是 Silicon Graphics, Inc. 的注册商标
- macOS 是 Apple Computers, Inc. 的注册商标

## 版权

FLTK 版权所有 1998-2025 Bill Spitzak 及其他贡献者，更多信息见 CREDITS.txt。

本库是自由软件。分发和使用权利写在随本文件提供的 “COPYING” 文件中。如果该文件缺失或损坏，请查看：

  https://www.fltk.org/COPYING.php

## 更多信息

更多内容见 README.txt：
https://github.com/fltk/fltk/blob/master/README.txt

![Build](https://github.com/fltk/fltk/actions/workflows/build.yml/badge.svg)

[![Build FLUID User Manual.](https://github.com/fltk/fltk/actions/workflows/build_fluid_docs.yml/badge.svg)](https://github.com/fltk/fltk/actions/workflows/build_fluid_docs.yml)
