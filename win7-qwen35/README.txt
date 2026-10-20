Qwen3.5-2B (Q4_K_M) 在 Win7 SP1 64 位上纯 CPU 运行
=====================================================

文件说明
--------
  llama-cli.exe            对话/单轮测试程序（静态链接，不需要额外 DLL）
  llama-bench.exe          测速程序
  Qwen3.5-2B-Q4_K_M.gguf   模型，1,280,835,840 字节（约 1.19 GiB）
                           sha256 aaf42c8b7c3cab2bf3d69c355048d4a0ee9973d48f16c731c0520ee914699223
                           来源：huggingface.co/unsloth/Qwen3.5-2B-GGUF
  prompt.txt               测试提示词（UTF-8），可用记事本修改
  llama-server.exe         网页聊天界面（UI）
  run.bat / bench.bat      双击运行（命令行测试 / 测速）
  server.bat               双击启动网页界面
  src-info\                编译脚本和 Win7 补丁（用于复现编译）

llama.cpp：从源码编译，commit dd002b9（2026-10-20，约 b11750）。
Qwen3.5 从 b7990 起支持，本版本远高于最低要求。
（程序里显示 "build: b1-dd002b9"，b1 只是因为编译时用了浅克隆，可以忽略。）

一、运行（run.bat）
------------------
建议先把整个文件夹复制到本地硬盘（如 C:\Qwen35），从 U 盘直接加载会慢很多。
双击 run.bat。参数：4 线程、-c 1024、最多输出 256 个 token、
--reasoning off（关闭思考）、-st（单轮，回答完自动退出）。
加载模型一般需要 10~60 秒，之后开始输出回答。

如果中文显示成乱码：只是控制台字体的问题，不影响测试。
可以右键窗口标题栏 → 属性 → 字体，换成 TrueType 字体。
最后的速度行是纯英文数字，总能看清。

二、怎么看速度
--------------
run.bat：回答结束后会有一行
    [ Prompt: 45.2 t/s | Generation: 12.9 t/s ]
    Prompt = 提示词处理速度，Generation = 生成速度
bench.bat：输出一张表格，同时保存到 bench_result.txt
    pp128 那行 → 提示词处理 t/s
    tg64  那行 → 生成 t/s   ← 成功标准看这个：≥ 1 t/s
参考：在一台 4 核 AVX2 云主机上（Linux，同一份源码）pp 约 85~98 t/s，tg 约 13 t/s。
i5-6600 应该在同一量级；瓶颈多半是内存带宽（单通道还是双通道）。

二点五、网页界面（server.bat）
----------------------------
双击 server.bat，黑色窗口里出现 "server is listening on http://127.0.0.1:8080" 即启动完成，
约 20 秒后会自动用默认浏览器打开该地址；如果默认浏览器不是 Supermium，
就手动在 Supermium 地址栏输入 http://127.0.0.1:8080 。
每条回答下方会显示生成速度（xx t/s）。
上下文为 2048（网页界面每次会多带约 300 token，1024 太容易满）；聊久了变慢或报错时，
点左上角“新对话”即可。峰值内存实测仍约 1.4 GB（任务管理器里看 llama-server.exe）。
用完直接关闭黑色窗口即可停止。只监听本机 127.0.0.1，机房里其他电脑访问不到。
如果 Windows 防火墙弹窗询问，点“取消”也不影响本机使用。

三、峰值内存（任务管理器）
--------------------------
启动前先打开任务管理器（Ctrl+Shift+Esc）→“进程”选项卡 → 查看 → 选择列，
勾选“峰值工作集(内存)”（以及表示当前值的“工作集(内存)”）。然后运行 run.bat，
看 llama-cli.exe 的“峰值工作集”。实测约 1.4 GB。
整机情况：在“性能”选项卡看“物理内存”使用率和“可用”。
如果“可用”一直接近 0、硬盘灯常亮，说明在用页面文件（也就是“明显卡顿”）。
4GB 机器上请先关掉浏览器等其他程序。

备用方案：内存紧张时，把 run.bat 里的 -c 1024 改成 -c 512。
如果程序一闪就退出，请在 cmd 里运行来查看报错：
    cd /d C:\Qwen35
    llama-cli.exe -m Qwen3.5-2B-Q4_K_M.gguf -p "hello" -t 4 -c 1024 --reasoning off -st
尤其是出现“无法定位程序输入点 ... 于动态链接库 KERNEL32.dll”这类报错，说明缺系统函数，
请把完整报错记下来。

模型下载（未放入仓库，超出 GitHub 100MB 限制）
----------------------------------------------
https://huggingface.co/unsloth/Qwen3.5-2B-GGUF/resolve/main/Qwen3.5-2B-Q4_K_M.gguf
下载后放到本文件夹，核对 sha256：
    certutil -hashfile Qwen3.5-2B-Q4_K_M.gguf SHA256
应为 aaf42c8b7c3cab2bf3d69c355048d4a0ee9973d48f16c731c0520ee914699223
