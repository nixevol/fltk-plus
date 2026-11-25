FLUID 语言包
============

英文是内置的，不用做语言包。其他语言在这个目录放一个 UTF-8 的 JSON 文件即可。文件名随意，只要以 .json 结尾。设置 → 常规 → Language 会列出每一个读成功的语言包。

FLUID language packs
=====================

English is built in. Add another language by placing a UTF-8 JSON file in this directory. The file name can be anything ending in .json. FLUID lists every valid pack in Settings > General > Language.

A pack looks like this:

{
  "id": "zh-CN",
  "name": "简体中文",
  "translations": {
    "&File": "文件(&F)"
  }
}

id is the value saved in preferences. name is the text shown in the dropdown. Each key in translations is the original English interface string, including the & shortcut marker. Missing keys stay in English.

The program looks for packs in this order:

1. The directory in the FLUID_LOCALE_DIR environment variable
2. locale next to the fluid executable
3. ../share/fltk-plus/locale relative to the executable

This works on Windows, Linux, and macOS.
