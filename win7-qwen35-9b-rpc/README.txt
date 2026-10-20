﻿Qwen3.5-9B (Q4_K_M) 在多台 Win7 机器上分布式运行（局域网 RPC，纯命令行，每台都能提问）
=========================================================================================

原理
----
模型 5.68 GB，单台 4GB 内存放不下。llama.cpp 的 RPC 功能把模型按层拆到多台机器：
  工作机：运行 worker.bat（ggml-rpc-server），把本机 CPU 和内存借给主机用。
  主机  ：运行 main.bat（llama-server，无网页界面），读取模型、把各层分发给工作机，
          并在局域网 8080 端口提供问答服务。
  提问  ：任何一台机器（主机、工作机都可以）运行 ask.bat，在命令行里提问。
每生成一个 token，数据依次经过各台机器，同一时刻只有一台在计算。所以多台机器的作用是
“凑够内存”，速度不会因为机器多而变快（大约等于单台机器跑 9B 的速度，再加一点网络开销）。

两种方案（机器配置完全相同）
--------------------------
                     方案 A（推荐）           方案 B（最少）
  机器数             4 台：主机 + 3 台工作机   3 台：主机 + 2 台工作机（主机也分担层）
  每台工作机内存     约 1.7~1.9 GB             约 1.9~2.0 GB
  主机内存           约 1.8 GB（加载时峰值）   约 2.1 GB
  余量               宽裕                      紧张，要关掉其他所有程序
ask.bat（命令行客户端）本身只占几十 MB，在工作机上同时运行没问题。
以上内存是在云端用同一份源码模拟测得（方案 A 含 3 人同时提问），Win7 上可能略有差异。

速度（云端模拟，方案 A）
  1 人提问：生成约 2.0 t/s，提示词处理约 7 t/s
  3 人同时提问：每人约 1.3~1.6 t/s（合计约 4.4 t/s）
  i5-6600 实际速度主要取决于内存是否双通道。

文件（每台机器都放整个文件夹；模型只需放在主机上）
--------------------------------------------------
  config.bat            配置：主机 IP、工作机 IP、分配比例、同时提问人数（每台机器内容相同）
  worker.bat            工作机运行
  main.bat              主机运行（后台服务，无界面）
  ask.bat               任何机器运行：多轮对话（不思考）
  ask_think.bat         任何机器运行：多轮对话（思考，最多 400 token）
  test.bat              任何机器运行：单轮测速（用 prompt.txt），最后打印 t/s
  ggml-rpc-server.exe / llama-server.exe / llama-cli.exe
  Qwen3.5-9B-Q4_K_M.gguf  模型，只放主机
      下载：https://huggingface.co/unsloth/Qwen3.5-9B-GGUF/resolve/main/Qwen3.5-9B-Q4_K_M.gguf
      5,680,522,464 字节，sha256 03b74727a860a56338e042c4420bb3f04b2fec5734175f4cb9fa853daf52b7e8
      核对：certutil -hashfile Qwen3.5-9B-Q4_K_M.gguf SHA256

注意：模型超过 4 GB，FAT32 格式的 U 盘拷不进去！
     U 盘请格式化为 exFAT 或 NTFS（Win7 SP1 都能读）。

步骤
----
0. 准备：所有机器在同一个局域网（同一网段，如 192.168.1.x）。
   每台机器把整个文件夹放到不被还原卡还原的分区（如 D:\Qwen9B）。
1. 每台工作机：双击 worker.bat。窗口顶部会显示本机 IPv4 地址，记下来。
   出现 "Starting RPC server" 后保持窗口打开。
2. 在主机上用 ipconfig 查看主机 IP。用记事本打开 config.bat：
      SERVER  = 主机 IP
      WORKERS = 各工作机 IP（端口都是 50052）
   保存后，把这个 config.bat 复制到每台机器的文件夹里（提问时要用 SERVER）。
3. 主机：双击 main.bat。第一次启动要把约 5 GB 权重通过网络发给工作机：
      千兆网约 1~2 分钟，百兆网约 8~10 分钟。
   工作机会把收到的权重缓存在自己的 rpc-cache 文件夹（约 1.6 GB），之后再启动就快很多。
   出现 "server is listening on http://0.0.0.0:8080" 后，保持窗口打开。
4. 提问：在任何一台机器上双击 ask.bat，看到 "> " 后输入问题、回车。
      /clear  开始新对话（清空上下文）
      /exit 或 Ctrl+C  退出
   每条回答后会显示 [ Prompt: xx t/s | Generation: xx t/s ]。
   最多可以 USERS 个人（默认 3）同时提问，多出来的人会排队等待。
5. 结束：关闭主机和各工作机的黑色窗口。

思考模式：默认关闭（9B 默认会思考，以 2 t/s 的速度要等很久）。
需要时用 ask_think.bat（思考最多 400 token，约 3 分钟后作答）。每台机器可以各自选择。

峰值内存：每台机器都在任务管理器 →“进程”里看“峰值工作集(内存)”
（工作机看 ggml-rpc-server.exe，主机看 llama-server.exe），
再到“性能”选项卡确认“可用”没有长时间接近 0。

常见问题
--------
- 主机加载失败 / 卡住，或者其他机器 ask.bat 报 "failed to connect"：
  多半是防火墙拦截了端口。需要放行：
    工作机：TCP 50052 入站      主机：TCP 8080 入站
  Win7 防火墙放行需要管理员权限，请管理员在对应机器的“管理员命令提示符”里执行：
    工作机： netsh advfirewall firewall add rule name="llama-rpc" dir=in action=allow protocol=TCP localport=50052
    主机：   netsh advfirewall firewall add rule name="llama-server" dir=in action=allow protocol=TCP localport=8080
  主机上自己运行 ask.bat 时只需改 SERVER=127.0.0.1 就不受防火墙影响（可用来先验证集群本身是否正常）。
  另外，main.bat 里的 ping 检查只是参考：Win7 默认屏蔽 ping，ping 不通不一定是网络不通。
- 某台工作机内存不够：调整 config.bat 里的 TS 比例（数字越大分到越多）。
  方案 A 的 12,12,7 是为了让三台平均：最后一台还要放输出层，所以分得少一些。
  同时提问的人越多，每台机器多占一点内存（每人约 +30 MB），可以把 USERS 调小。
- 修改 TS 或 NGL 后，工作机缓存的权重对不上会重新传一次，属于正常现象。
- 中文显示成乱码：只是控制台字体问题，右键标题栏 → 属性 → 字体，换成 TrueType 字体。
  输入中文需要用输入法正常输入；如果输入中文后回答异常，可以先用英文提问测试。
- 安全：ggml-rpc-server（50052）和 llama-server（8080）都没有密码验证，
  局域网内任何人都能连接。只在可信的机房局域网里使用，用完立刻关闭窗口。

编译说明：与 ../win7-qwen35 相同的源码（llama.cpp dd002b9）和 Win7 补丁，额外加了 -DGGML_RPC=ON；
并给 llama-cli 打了一个小补丁（src-info/win7-patches.patch 里的 tools/cli/cli-context.cpp 部分）：
连接外部服务器（--server-base）时把 --reasoning、--reasoning-budget、-n 转发给服务器，
否则客户端无法单独开关思考。检查过没有新增 Win7 不支持的系统函数。
