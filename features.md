# Features

## Purpose

记录 `my_shell_protect` 当前保护壳能力、开发状态与后续交付计划，方便持续开发 x64 PE 加壳、解壳、压缩和兼容性修复。

## Status Rules

- `planned`：已明确需求，尚未开始。
- `in progress`：正在实现或验证。
- `implemented`：已完成并通过基础测试。
- `deferred`：暂缓，保留原因。
- `dropped`：放弃，保留原因。

## Current Capabilities

### x64 PE 加壳主流程

- 状态：`implemented`
- 目标：对 AMD64/PE32+ 可执行文件增加 `.VMP` 壳区段，保存 OEP，压缩原始区段并在运行时恢复。
- 用户可见结果：`CombatShellCli.exe pack <target.exe>` 可生成受保护文件，`unpack` 可通过备份恢复。
- 技术方案：`ShellCliMain.cpp` 驱动 `AddSection.cpp`、`CompressionData.cpp`、`studData.cpp`；`CombatShell/CombatShell.cpp` 作为运行时 stub 恢复区段和 IAT。
- 主要代码区域：`ShellCliMain.cpp`、`AddSection.cpp`、`CompressionData.cpp`、`studData.cpp`、`CombatShell/CombatShell.cpp`。
- 风险/问题：仍有历史 x86 指针截断警告；x64 VM 指令数量仍依赖固定分析边界。
- 后续：继续扩展 TLS、异常表、Load Config、CFG 等更复杂 PE 字段样本测试。

### 压缩算法选项

- 状态：`implemented`
- 目标：允许加壳时选择区段载荷编码方式。
- 用户可见结果：`pack` 支持 `--compress=quicklz`、`--compress=lz4`、`--compress=none`，并提供 `--no-compress` 别名。
- 技术方案：在 `_Stud` 中记录 `s_CompressionMethod`；打包端按算法写入连续区段数据；运行时 stub 根据方法分派 QuickLZ、LZ4 或原样拷贝恢复。
- 主要代码区域：`CombatShell/CombatShell.h`、`CompressionData.cpp`、`CombatShell/CombatShell.cpp`、`ShellCliMain.cpp`、`lz4/include/lz4.c`。
- 风险/问题：LZ4 需要保持 shell payload 自包含，已为 MSVC 构建内联本地 `memmove`，避免运行时依赖 VCRUNTIME 的 `memmove` 导入。
- 后续：为更多不可压缩/大体积区段加入回归样本。

### 保护和混淆选项

- 状态：`implemented`
- 目标：让加壳时可显式组合入口 VM 虚拟化和区段载荷混淆。
- 用户可见结果：`pack` 支持 `--vm`、`--no-vm`、`--encrypt-sections`、`--no-encrypt-sections`、`--xor-key=<hex|dec>`。
- 技术方案：在 `_Stud` 中记录 `s_ProtectionFlags` 和 `s_EncryptionKey`；打包端对压缩/原样载荷做 XOR；运行时在解压前原地解密；x64 默认开启入口 VM，`--no-vm` 可回退到直接入口 stub。
- 主要代码区域：`CombatShell/CombatShell.h`、`CompressionData.cpp`、`CombatShell/CombatShell.cpp`、`ShellCliMain.cpp`。
- 风险/问题：当前 XOR 是轻量混淆，不等价于高强度密码学保护；VM 仍主要覆盖壳入口固定指令序列。
- 后续：扩展自动 VM 边界识别、导入表混淆、字符串混淆和反 dump 选项。

### x64 目标格式保护

- 状态：`implemented`
- 目标：避免使用 x64 构建误处理非 AMD64 目标，降低跨架构破坏风险。
- 用户可见结果：x64 CLI 对非 AMD64 目标会报错并停止。
- 技术方案：`RunPack` 读取 PE Machine 字段并按当前构建架构校验。
- 主要代码区域：`ShellCliMain.cpp`。
- 风险/问题：Win32 构建路径仍保留历史实现，未在本轮重点验证。
- 后续：增加 PE OptionalHeader Magic、NumberOfRvaAndSizes、节表容量等更严格校验。

## Planned Features

### PE 字段兼容性回归集

- 状态：`planned`
- 目标：覆盖 x64 常见字段组合：TLS、异常目录、延迟导入、重定位、Load Config、资源和不同 FileAlignment/SectionAlignment。
- 用户可见结果：开发者可一键运行多样本 pack/run/unpack 回归。
- 技术方案：扩展 `examples/` 样本和 `scripts/run-pack-unpack-test.ps1` 参数矩阵。
- 主要代码区域：`examples/`、`scripts/`。
- 风险/问题：部分 GUI/系统样本需要进程存活判定而非 stdout。
- 后续：先补 TLS/异常表样本。

## Technical Principles

- 优先保证 x64 AMD64/PE32+ 主链路稳定，再扩展 Win32。
- 加壳修改保持可逆：默认生成 `old_<target>` 备份，`unpack` 优先备份恢复。
- 壳 stub 尽量自包含，避免依赖未复制的 DLL 导入、CRT 初始化或外部状态。
- 每次改动至少验证 build、pack、run、unpack 四步闭环。

## Delivery Plan

1. 已完成：压缩算法 CLI 选项与 x64 LZ4/QuickLZ/none 分派。
2. 已完成：入口 VM 开关、区段载荷 XOR 混淆和 key 配置。
3. 下一步：补充 PE 字段兼容性样本和脚本矩阵。
4. 后续：清理 x64 编译警告，替换固定 VM 指令数量为自动边界识别。

## Change Log

- 2026-05-17：新增保护和混淆选项记录；入口 VM 可开关，区段载荷支持 XOR 混淆与 key 配置。
- 2026-05-17：创建功能跟踪；记录 x64 加壳主链路、压缩选项、架构校验和后续 PE 字段兼容计划。
