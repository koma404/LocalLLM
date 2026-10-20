Qwen3.5-9B (Q4_K_M) 在多台 Win7 机器上分布式运行（局域网 RPC）
================================================================

原理
----
模型 5.68 GB，单台 4GB 内存放不下。llama.cpp 的 RPC 功能把模型按层拆到多台机器：
  工作机：运行 worker.bat（ggml-rpc-server），把本机 CPU 和内存借给主机用。
  主机  ：运行 main.bat（llama-server），读取模型文件，把各层分发给工作机，并提供网页界面。
每生成一个 token，数据依次经过各台机器，同一时刻只有一台在计算。所以多台机器的作用是
“凑够内存”，速度不会因为机器多而变快（大约等于单台机器跑 9B 的速度，再加一点网络开销）。

两种方案（机器配置完全相同）
--------------------------
                     方案 A（推荐）           方案 B（最少）
  机器数             4 台：主机 + 3 台工作机   3 台：主机 + 2 台工作机（主机也分担层）
  每台工作机内存     约 1.7~1.8 GB             约 1.9 GB
  主机内存           约 1.8 GB（加载时峰值）   约 2.1 GB
                     + Supermium 几百 MB       + Supermium 几百 MB
  余量               宽裕                      紧张，要关掉其他所有程序
在 config.bat 里切换方案（默认是 A，B 那几行去掉 rem 即可，同时给 A 那几行加上 rem）。
以上内存是在云端用同一份源码模拟测得，Win7 上可能略有差异。
模拟测试的生成速度约 2.1 t/s，提示词处理约 7~9 t/s；i5-6600 实际速度主要取决于内存是否双通道。

文件
----
  ggml-rpc-server.exe   工作机程序
  llama-server.exe      主机程序（网页界面）     llama-cli.exe  主机命令行测试
  worker.bat            工作机双击运行
  config.bat            主机配置：填写工作机 IP 和分配比例
  main.bat              主机双击运行（网页界面）
  test.bat              主机单轮命令行测试，最后打印 t/s
  prompt.txt            test.bat 用的提示词
  Qwen3.5-9B-Q4_K_M.gguf  模型，只需放在主机上（工作机不需要）
      下载：https://huggingface.co/unsloth/Qwen3.5-9B-GGUF/resolve/main/Qwen3.5-9B-Q4_K_M.gguf
      5,680,522,464 字节，sha256 03b74727a860a56338e042c4420bb3f04b2fec5734175f4cb9fa853daf52b7e8
      核对：certutil -hashfile Qwen3.5-9B-Q4_K_M.gguf SHA256

注意：模型超过 4 GB，FAT32 格式的 U 盘拷不进去！
     U 盘请格式化为 exFAT 或 NTFS（Win7 SP1 都能读）。

步骤
----
0. 准备：所有机器在同一个局域网（同一网段，如 192.168.1.x）。
   每台机器把整个文件夹放到不被还原卡还原的分区（如 D:\Qwen9B）；模型只放主机。
1. 每台工作机：双击 worker.bat。窗口顶部会显示本机 IPv4 地址，记下来。
   出现 "Starting RPC server" 后保持窗口打开。
   如果弹出 Windows 防火墙提示，需要选“允许访问”（这一步需要管理员权限，见下方“常见问题”）。
2. 主机：用记事本打开 config.bat，把 WORKERS 改成工作机的 IP（端口都是 50052），保存。
3. 主机：双击 main.bat。第一次启动要把约 5 GB 权重通过网络发给工作机：
      千兆网约 1~2 分钟，百兆网约 8~10 分钟。
   工作机会把收到的权重缓存在自己的 rpc-cache 文件夹（约 1.6 GB），之后再启动就快很多。
   出现 "server is listening on http://127.0.0.1:8080" 后，用 Supermium 打开这个地址。
4. 测速：网页里每条回答下方会显示 t/s；或者关掉 main.bat 后运行 test.bat，看最后一行
   [ Prompt: xx t/s | Generation: xx t/s ]。
5. 结束：关闭主机和各工作机的黑色窗口。

思考模式：默认关闭（9B 默认会思考，以 2 t/s 的速度要等很久）。
需要时在网页输入框旁的灯泡按钮选 Low（最多 512 token）。

峰值内存：每台机器都在任务管理器 →“进程”里看“峰值工作集(内存)”
（工作机看 ggml-rpc-server.exe，主机看 llama-server.exe），
再到“性能”选项卡确认“可用”没有长时间接近 0。

常见问题
--------
- 主机报连接失败 / 卡在加载：
  多半是工作机的防火墙拦截了 50052 端口。Win7 防火墙放行程序需要管理员权限，
  机房里请让管理员在工作机上执行一次（管理员命令提示符）：
    netsh advfirewall firewall add rule name="llama-rpc" dir=in action=allow protocol=TCP localport=50052
  或者让管理员关闭防火墙。另外，main.bat 里的 ping 检查只是参考：
  Win7 默认屏蔽 ping，ping 不通不一定是网络不通。
- 某台工作机内存不够：调整 config.bat 里的 TS 比例（数字越大分到越多）。
  方案 A 的 12,12,7 是为了让三台平均：最后一台还要放输出层，所以分得少一些。
- 修改 TS 或 NGL 后，工作机缓存的权重对不上会重新传一次，属于正常现象。
- 安全：ggml-rpc-server 没有任何密码验证，局域网内任何人都能连接并让它执行计算。
  只在可信的机房局域网里使用，用完立刻关闭窗口。

编译说明：与 ../win7-qwen35 相同的源码（llama.cpp dd002b9）和 Win7 补丁，
额外加了 -DGGML_RPC=ON；检查过没有新增 Win7 不支持的系统函数。
