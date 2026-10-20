﻿Qwen3.5-9B (Q4_K_M) 机房共享盘一键集群（Win7，局域网 RPC，纯命令行）
=====================================================================

整个文件夹放在共享盘上（例如 Z:\qwen9b，即 \\jsa22\share\qwen9b）。
  主机（固定 1 台）   ：双击 start_main.bat
  工作机（固定 2~4 台）：双击 start_worker.bat
  其他任何机器       ：双击 open_ui.bat 打开网页聊天界面（或 ask.bat 命令行），不用拷贝、不用配置
不需要填写任何 IP：工作机启动时把自己的 IP 登记到 cluster\workers\，
主机启动时自动收集在线的工作机、自动计算分配比例，并把自己的 IP 写到 cluster\server.txt，
open_ui.bat / ask.bat 读这个文件就知道该连哪台主机。

一、第一次准备（管理员做一次）
----------------------------
1. 把整个文件夹复制到共享盘，例如 Z:\qwen9b。
2. 把模型放进 Z:\qwen9b\model\
      下载：https://huggingface.co/unsloth/Qwen3.5-9B-GGUF/resolve/main/Qwen3.5-9B-Q4_K_M.gguf
      5,680,522,464 字节，sha256 03b74727a860a56338e042c4420bb3f04b2fec5734175f4cb9fa853daf52b7e8
      核对：certutil -hashfile Qwen3.5-9B-Q4_K_M.gguf SHA256
   注意：模型超过 4 GB，FAT32 格式的 U 盘拷不进去，要用 exFAT / NTFS 的 U 盘，或者直接通过网络复制。
3. 用记事本打开 cluster\settings.bat，确认：
      LOCAL_DIR  每台机器的本地文件夹，必须在不被还原卡还原的分区（默认 D:\qwen9b）
                 主机在这里放一份模型的本地副本，工作机在这里放权重缓存（约 1.6 GB）
      USERS      同时提问的人数（默认 3）
      BROWSER（可选）  open_ui.bat 用的浏览器完整路径，例如 Supermium 的 chrome.exe；
                 留空则用系统默认浏览器
      MAIN_PC / WORKER_PCS（可选）填计算机名后，只有这些机器能当主机 / 工作机，
                 防止别人误点 start_worker.bat。计算机名用 hostname 命令查看。
4. 在主机和每台工作机上，右键 setup_firewall.bat →“以管理员身份运行”一次
   （放行 8080 和 50052 端口）。提问的机器不需要做这一步。
5. 共享盘权限：主机和工作机需要对 cluster 文件夹有写权限；提问的机器只需要读权限。

二、每次使用
------------
1. 每台工作机：双击 start_worker.bat。显示 "Registered ... Keep this window open" 就是登记好了。
2. 主机：双击 start_main.bat。
     [1/3] 第一次会把 5.7 GB 模型从共享盘复制到本地（之后就不用了）
     [2/3] 列出登记过的工作机，OK = 在线，not reachable = 不在线（自动跳过）
     [3/3] 自动选择方案并启动：
           3 台以上工作机：模型全部放在工作机上（推荐 3 台）
           2 台工作机    ：主机自己也分担一部分（内存更紧张）
           少于 2 台     ：每 20 秒重新检查，直到够数
     第一次启动要把约 5 GB 权重发给工作机（千兆网约 1~2 分钟，百兆网约 8~10 分钟），
     工作机缓存后，下次就快很多。出现 "listening on http://0.0.0.0:8080" 就可以提问了。
3. 提问：任何机器打开 Z:\qwen9b，双击
      open_ui.bat     图形界面：自动等主机就绪，然后用浏览器打开 http://主机IP:8080
                      （如果没自动打开，就在 Supermium 地址栏手动输入窗口里显示的地址）
                      输入框旁的灯泡按钮可开关思考（Default/Off = 不思考，Low = 最多 512 token）；
                      每条回答下方显示速度；对话记录保存在各自浏览器里，左侧栏铅笔图标 = 新对话
      ask.bat         多轮对话（不思考）。/clear 新对话，/exit 退出
      ask_think.bat   多轮对话（思考，最多 400 token，约 3 分钟后作答）
      test.bat        单轮测速，最后一行 [ Prompt: xx t/s | Generation: xx t/s ]
   如果主机还在加载，open_ui.bat / ask.bat 会自动等待（最多 5 分钟）。
   最多 USERS 个人同时生成，多出来的人会排队。
4. 结束：关闭主机和工作机的黑色窗口。

三、自动恢复（看门狗）
--------------------
任何一台工作机关掉或断网后，下一次有人提问时主机会报错退出；
start_main.bat 会在 10 秒后自动重新扫描在线的工作机，并用剩下的机器重新启动（需要重新加载几分钟）。
掉线的工作机重新双击 start_worker.bat 后，要等主机下一次重启才会被用上；
想立刻用上，就在主机窗口按 Ctrl+C 退出，再重新运行 start_main.bat。

四、速度和内存（云端模拟测得，Win7 真机可能略有差异）
-----------------------------------------------------
  3 台工作机：1 人提问约 2.0 t/s，3 人同时提问每人约 1.3~1.6 t/s
  2 台工作机：约 1.85 t/s
  峰值内存：工作机约 1.7~1.9 GB（2 台工作机时约 2.0 GB），主机约 1.8 GB（2 台工作机时约 2.1 GB）
  提问的机器：ask.bat 只占几十 MB；网页界面占的是浏览器内存（Supermium 一个标签页几百 MB）。
  每台机器在任务管理器 →“进程”里看“峰值工作集(内存)”：
  工作机看 ggml-rpc-server.exe，主机看 llama-server.exe。
多台机器是为了凑够内存，速度不会因为工作机多而变快（工作机越多，网络往返越多，反而略慢）。

五、常见问题
------------
- open_ui.bat / ask.bat 一直显示 Waiting / 5 分钟后报 Cannot reach：
  主机没启动、还在加载，或者主机的 8080 端口被防火墙拦截（主机上运行 setup_firewall.bat）。
- 主机扫描时工作机显示 not reachable，但工作机窗口明明开着：
  工作机的 50052 端口被防火墙拦截（工作机上运行 setup_firewall.bat），或者不在同一网段。
- start_worker.bat 报 Cannot write：该机器对共享盘的 cluster\workers 没有写权限。
- 集群运行期间，不要修改或覆盖共享盘上的 .bat 文件！
  cmd 是边读边执行批处理的，文件内容一变，正在运行的窗口会读到错乱的命令。
  要更新文件，先关掉所有窗口再改。
- 中文显示成乱码：控制台字体问题，右键标题栏 → 属性 → 字体，换成 TrueType 字体。
- 安全：8080 和 50052 端口都没有密码验证，局域网内任何人都能连接。
  只在可信的机房局域网里使用，用完关闭窗口。

六、文件
--------
  start_main.bat / start_worker.bat / open_ui.bat / ask.bat / ask_think.bat / test.bat / setup_firewall.bat
  prompt.txt                test.bat 用的提示词
  bin\                      ggml-rpc-server.exe、llama-server.exe、llama-cli.exe、qwen-helper.exe
  cluster\settings.bat      设置
  cluster\workers\          工作机自动登记（每台一个 计算机名.txt）
  cluster\server.txt        主机自动写入的 IP
  model\                    放模型
  src-info\                 补丁和 qwen-helper 源码

编译说明：llama.cpp dd002b9，MinGW-w64 GCC 13，_WIN32_WINNT=0x0601，静态链接，-DGGML_RPC=ON。
补丁（src-info\win7-patches.patch）：
  1) cpp-httplib：把 Win8 才有的 3 个函数换成 Win7 的等价函数；
  2) llama-cli：连接外部服务器时转发 --reasoning、--reasoning-budget、-n。
qwen-helper.exe（src-info\qwen-helper.c）是给批处理用的小工具：取本机 IP、探测端口、
检查服务状态、根据在线工作机数量生成分配方案。只用到 Win7 自带的系统函数。
测试：除云端 Linux 模拟外，还在 Wine（Win7 SP1 模式）里实际运行了这些 Windows 程序和批处理：
登记、自动扫描（跳过离线机器）、加载、从“共享盘”提问、工作机掉线后自动切换到 2 台方案恢复，都已验证。
