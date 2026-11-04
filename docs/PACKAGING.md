# 打包与体积检查

发布构建明确使用 Release，保留 Qt WebEngine 内嵌 CAS 登录。Qt/Chromium 运行库是主要体积来源；不会删除 WebEngine 辅助进程、ICU、DevTools 资源、图形兼容库或 TLS 依赖。

部署脚本要求全新输出目录，已有目录时直接报错，不合并旧包。Python 3 标准库工具 `scripts/package_tools.py` 从核心压缩包中仅提取 `zju-connect` 程序（Windows 另由仓库提供 Wintun），不解包文档等无关内容。

部署完成后，仅裁剪发布包中的 QML 调试插件、PDB/dSYM 和其他语言的 Qt 翻译；保留简体中文、繁体中文、英文及 WebEngine 的对应语言资源。Linux 在 AppDir 内裁剪后再生成 AppImage，macOS 在裁剪及单架构处理后重新做 ad-hoc 签名并验证；正式 Developer ID 签名及公证应在此之后执行。Windows 便携包沿用 README 中的 VC Redistributable 安装前提。

每个平台的 CI 另行上传 `size-*` 报告 artifact，不放入应用运行目录：

- `package-pruning.json`：裁剪前后文件字节数和逐项删除清单。
- `package-size.json`：文件清单、分类体积、最大文件、最终压缩包字节数、Qt 版本、核心版本及 SHA-256，以及部署工具 SHA-256。

比较两个构建时，应使用相同架构、Qt 版本、核心版本/哈希以及部署工具版本。常规构建仍下载上游 latest/nightly 核心，报告记录实际下载版本；不能把更换核心造成的差异归为裁剪收益。GitHub artifact 传输大小也不等同于应用 ZIP/AppImage 文件大小或解压体积。

本地可独立运行 `python tests/package_tools_test.py`，使用合成目录验证资源保留、语言裁剪及核心 ZIP 提取。CMake 在找到 Python 3 时也将该测试注册进 CTest。

发布前需在无 Qt 开发环境的系统验证内嵌登录、验证码、TLS 和非中文系统的英文语言回退；Linux 补测 X11/Wayland，macOS 检查签名。真实 CAS/TOTP 登录需要账号参与，构建通过不能替代这些验证。
