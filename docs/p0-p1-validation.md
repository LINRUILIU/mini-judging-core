# P0 / P1 修复验收记录

日期：2026-09-20。修复基于 `28ca0fd`。本次接续已有工作区的 P0/P1
修补，在独立分支审阅、补充报告目录覆盖回归并重新构建验收。

## 缺陷、修复和回归覆盖

| 优先级 | 原问题 | 修复 | 回归验证 |
|---|---|---|---|
| P0 | 缺失行与字面量 `<missing>` 混淆，错误接受 | 先比较共同存在的行，再比较行数 | `test_main.cpp`：缺失行、字面量、三种比较模式示例 |
| P0 | 同步写 stdin 可能被不读输入的程序阻塞 | 输入预写到独立临时文件，按块检查 deadline，子进程继承只读句柄 | `test_runner.cpp`：2 MiB 输入不读取、提前退出、先输出 64 KiB 再读输入 |
| P0 | 收尾阻塞读管道，越过时限和输出预算 | 只读当前可用字节，整个采集循环检查时限和共享预算，进程清理有界 | `test_runner.cpp`：无限输出、超时、后代持有输出句柄 |
| P0 | 输出恰好 N 字节被判 OLE | 接受 N，只在观察到第 N+1 字节时判超限 | `test_runner.cpp`：63 / 64 / 65 字节，以及双流合计边界 |
| P0 | stderr 污染答案、只终止直接进程 | 分离双流，仅比较 stdout；暂停创建后关联 Job，再恢复运行，结束时回收进程树 | `test_runner.cpp`：双流分离、stderr 超限、后代 PID 退出检查 |
| P0 | Release 的 `assert` 检查被移除 | 使用运行时检查，CTest 反向验证故意失败的检查 | `mini_judge_check_failure`：Debug / Release 都必须检测到失败 |
| P0 | 集成测试只看 CLI 退出码 | 解析实际报告，核对六种状态、计数和得分 | `test_cli.ps1`：AC / WA / TLE / RE / OLE / SE、JSON 字段、CSV 行数、HTML 产物 |
| P1 | 并发启动时句柄继承范围过大 | `STARTUPINFOEX` 显式列出三个标准流句柄 | `test_runner.cpp`：8 个并发输入回显、后代清理 |
| P1 | 无界 LIFO、任务异常逸出、取消语义不明 | 有界 FIFO、提交背压、工作线程捕获异常、等待后重抛、取消待执行任务并唤醒提交者 | `test_scheduler.cpp`：队列容量、顺序、异常后继续、取消唤醒；`test_engine.cpp`：单用例失败继续汇总 |
| P1 | 数字范围、完整整数语法和唯一 ID 缺检查 | 配置和直接引擎调用均校验限制及 ID；拒绝重复键、损坏 UTF-8 和错误路径 | `test_config.cpp`、`test_engine.cpp` 和 CLI 非法参数用例 |
| P1 | 报告写失败仍提示成功 | 检查打开、写入、flush 和 close，CLI 以非零退出报告失败 | `test_reporter.cpp`、`test_cli.ps1`：目录占用目标文件时必须失败，检查转义内容 |
| P1 | `--report-dir` 覆盖前就创建、检查旧报告目录 | 配置加载只解析路径；CLI 应用覆盖后准备唯一选中的目录 | `test_config.cpp`：加载不创建目录；`test_cli.ps1`：不触碰旧目录/文件、中文空格路径、未覆盖的错误路径仍失败 |

同时修复多配置构建的样例路径，给每次执行分配独立工作目录，并让
CLI 使用 Windows 宽字符参数，覆盖中文和空格路径。

## 本次测试弹窗的处理

最初的 SE 测试尝试启动一个写有普通文本的 `.exe`，触发 Windows
“不支持的 16 位应用程序”弹窗。该夹具已改为对合法可执行文件加独占锁，
制造可诊断的读取/启动失败。执行器也加入启动前类型检查及局部错误模式处理。

另有专项回归向执行器传入文本 `.exe`，断言其在类型检查阶段返回 SE；
正常启动后还会检查调用线程的错误模式已恢复。最终专项验证没有出现弹窗。

## 独立分支的验证环境与结果

环境：Windows、CMake 4.2.3、PowerShell 7。源代码和构建目录均包含中文用户名。
以下三组都是包含报告目录覆盖修复的本次完整运行，未沿用原工作区的测试成绩。

| 编译器 | 生成器 / 构建模式 | 完整 CTest | 总耗时 |
|---|---|---|---|
| MSVC 19.51.36252 | Ninja Multi-Config / Debug | 9/9 通过 | 5.82 秒 |
| MSVC 19.51.36252 | Ninja Multi-Config / Release | 9/9 通过 | 9.26 秒 |
| GNU MinGW 9.2.0 | Ninja / Release | 9/9 通过 | 13.71 秒 |

报告目录回归先在修复前的 Debug 程序上失败，错误为
`Override created the unused report directory`；修复后完整测试通过。
测试还覆盖旧路径被普通文件占用时的有效覆盖，以及不提供覆盖时的正确失败。
耗时仅记录测试完成情况，不是性能基准或不同编译器的速度对比。

本机 MinGW Makefiles 配置成功，但编译时在包含中文的源文件路径报
`No rule to make target`。同一份源码切换 Ninja 后完成编译与测试；README
采用此次验证通过的 Ninja 命令。MSVC 经 `vcvars64.bat` 初始化后构建。

Windows CI 使用 Debug / Release 矩阵，并保存 JUnit、CTest 日志及样例
JSON/CSV/HTML 报告为构建产物。远端结果以对应提交的 Actions 运行记录为准。

在仓库根目录运行：

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

九个 CTest 入口涵盖比较器、运行时检查自检、配置、调度、报告、执行器、
样例 CLI、引擎汇总和完整 CLI 报告检查；它们不是只有九个输入样例。

## 行为变化与范围

stdin 是有限文件输入；stdout 和 stderr 合计受输出上限约束；程序在独立
临时目录运行。依赖原 exe 目录中的相对路径文件、或依赖管道类型 stdin
的程序需要适配。具体配置范围及取消语义见 [可靠性约定](reliability.md)。

本轮没有添加安全沙箱、内存限制或跨平台后端；报告写入也不是三文件原子事务。
未重新运行 744 个课程程序及其 23,808 个组合，历史数字不作为修复后测试结果。
