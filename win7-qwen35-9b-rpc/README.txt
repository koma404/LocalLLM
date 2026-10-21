Qwen3.5-9B (Q4_K_M) 机房共享盘一键集群（Win7，局域网 RPC，纯命令行）
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
   （可选，看图功能）再下载视觉文件也放进 model\：
      https://huggingface.co/unsloth/Qwen3.5-9B-GGUF/resolve/main/mmproj-F16.gguf
      918,166,080 字节，sha256 f70dc3509053962b0d0d3ee8a7eacebf5d60aa560cad78254ae8698516ae029f
      不放这个文件也能正常用，只是不能发图片。
   注意：模型超过 4 GB，FAT32 格式的 U 盘拷不进去，要用 exFAT / NTFS 的 U 盘，或者直接通过网络复制。
3. 用记事本打开 cluster\settings.bat，确认：
      LOCAL_DIR  每台机器的本地文件夹，必须在不被还原卡还原的分区（默认 D:\qwen9b）
                 主机在这里放一份模型的本地副本，工作机在这里放权重缓存（约 1.6 GB）
      USERS      同时提问的人数（默认 3）
      CTX_TOTAL  总上下文长度（默认 16384 token），所有人共用一个池子，一个人用时可以用满。
                 很省内存：每 1000 token 所有机器合计只多约 32 MB。想更长就改成 32768。
      VISION     看图功能：1 = 开（默认，需要 model\mmproj-F16.gguf），0 = 关
      IMAGE_TOKENS  每张图最多变成多少 token（默认 256）。越大看得越细，但越慢、越占上下文
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
           复制时会显示百分比。如果很慢，可以按 Ctrl+C 停掉，删掉 LOCAL_DIR 里不完整的文件，
           用资源管理器把 model 里的 gguf 复制到 LOCAL_DIR，再重新运行（会自动跳过复制）。
     [2/3] 列出登记过的工作机，OK = 在线，not reachable = 不在线（自动跳过）
     [3/3] 自动选择方案并启动：
           3 台以上工作机：模型全部放在工作机上（推荐 3 台）
           2 台工作机    ：主机自己也分担一部分（内存更紧张）
           少于 2 台     ：每 20 秒重新检查，直到够数
     第一次启动要把约 5 GB 权重发给工作机（千兆网约 1~2 分钟，百兆网约 8~10 分钟），
     主机窗口会显示进度条：  loading text_model [#########.....] 45%  120 s, about 150 s left
     每台工作机窗口会显示：  received  812 MB (network 812 MB, local cache 0 MB)  6.5 MB/s
     传完后工作机显示 "All weights received. This worker is now serving the main PC."
     进度条长时间不动、MB 数也不再增加，才是卡住了。
     工作机缓存后，下次就快很多。出现 "listening on http://0.0.0.0:8080" 就可以提问了。
3. 提问：任何机器打开 Z:\qwen9b，双击
      open_ui.bat     图形界面：自动等主机就绪，然后用浏览器打开 http://主机IP:8080
                      （如果没自动打开，就在 Supermium 地址栏手动输入窗口里显示的地址）
                      输入框旁的灯泡按钮可开关思考：
                        Default / Off = 不思考
                        Minimal = 最多 256 token（约 100 秒）← 日常问答推荐，本仓库新加的档位
                        Low = 512 / Medium = 2048 / High = 8192 / Max = 不限
                      思考预算用完时会插入一句 "OK, time is up..." 让模型马上转入回答
                      （实测不加这句时 256 偶尔会因思路被截断而答错）；
                      输入框旁的附件按钮可以上传图片（或直接粘贴截图），让模型看图回答；
                      每条回答下方显示速度；对话记录保存在各自浏览器里，左侧栏铅笔图标 = 新对话
      ask.bat         多轮对话（不思考）。/clear 新对话，/exit 退出
      ask_think.bat   多轮对话（思考，最多 256 token，约 2 分钟后作答）
      test.bat        单轮测速，最后一行 [ Prompt: xx t/s | Generation: xx t/s ]
   如果主机还在加载，open_ui.bat / ask.bat 会自动等待（最多 5 分钟）。
   最多 USERS 个人同时生成，多出来的人会排队。
4. 结束：关闭主机和工作机的黑色窗口。

二点四、系统提示词与工具
------------------------
系统提示词（所有人默认的“人设”）：编辑 cluster\system-prompt.txt（记事本即可），重启主机生效。
  网页界面：第一次打开界面的人自动用它；已经自己改过 System Message 的人保留自己的，
            在设置里点“恢复默认”就会换成这份。
  ask.bat / ask_think.bat：每次都会带上它。
  提示词会在主机每次启动后提前处理好，存到 LOCAL_DIR\cache\sys.bin（settings.bat 里 CACHE_PROMPT=1）：
    第一次启动时计算并保存，之后直接读取，第一句话不用再等提示词处理。
    提示词、模型或上下文大小一改，下次启动会自动重新计算。进度看 LOCAL_DIR\cache\warm.log。
    命令行（ask.bat）已在 Linux 上验证：恢复后首个请求只需处理 18 个 token，而不是 208 个。
    网页界面是否也命中这份缓存，要在真机上看首句速度确认。
  提示词越长，这一步越费时间，所以请尽量写短；同一对话后续轮次本来就会复用缓存。

工具（settings.bat 里 TOOLS=1，默认开启）：
  run_javascript  在浏览器的隔离沙盒里运行一小段 JavaScript，用于精确计算
  get_datetime    当前日期时间（浏览器的时钟）
  get_info        运行环境信息
  这三个只在使用者自己的浏览器里运行，碰不到任何文件，已设为“始终允许”，不会弹出授权提示。
  注意：9B 在“不思考”时几乎不会主动用工具（实测会直接心算，还会算错）；
  选 Minimal 或更高的思考档位时，计算题会主动调用 run_javascript（实测 4/4）。
  服务器端读写文件、执行命令的工具（--tools）有安全风险，没有开启。

二点五、集群状态窗口（status.bat）
--------------------------------
主机运行 start_main.bat 时会自动多开一个 "Qwen cluster status" 窗口；任何机器双击 status.bat
也能打开同样的窗口（只看，不会暂停机器）。每 3 秒刷新：
  Server ... : READY   users generating 2   waiting 0   speed 3.20 t/s total (1.60 t/s per user)
      generating = 正在生成的人数，waiting = 排队的人数，speed = 实时生成速度
  This PC (main): free ... MB   llama-server.exe ... MB      主机自己的内存
  每台工作机一行：
      STATE    waiting / receiving（在接收权重）/ serving（在工作）/ PAUSED / OFFLINE
      FREE     那台机器的可用内存（MB）
      PROC     ggml-rpc-server.exe 占用的内存（MB）
      DISK-IN  每秒从硬盘换入的页数；持续 > 100 说明那台机器在用虚拟内存（会非常慢）
      MS/STEP  那台机器每一步计算用的毫秒数（一步 = 每个正在生成的人各 1 个 token）
      WARNING  LOW-MEMORY（可用 < 200 MB）/ SWAPPING / SLOW（比其他机器慢 2.5 倍以上）
  只有 1 人在生成时，最后会显示每个 token 的耗时分解：
      Per token: 510 ms total = 429 ms compute on workers + 81 ms network/main/other
      如果 "network/main/other" 很大，瓶颈在网络或主机；如果某台的 MS/STEP 特别大，瓶颈就是那台。
      （只有 2 台工作机时主机也承担计算，这部分算在 other 里。）

自动暂停（cluster\settings.bat 里 AUTO_PAUSE=1，默认开启）：
  主机上的状态窗口发现某台工作机连续约 18 秒 LOW-MEMORY 或 SWAPPING，
  就把它记到 cluster\paused\ 里，并重启服务（不再用它），状态窗口的 Events 里会写明原因，
  那台工作机的窗口也会显示 "*** PAUSED by the main PC"。
  为了防止越暂停越少，暂停后剩下的工作机不能少于 MIN_WORKERS（默认 3），否则只提示不暂停；
  两次自动暂停至少间隔 2 分钟。
  恢复：在那台机器上腾出内存后，关掉窗口重新运行 start_worker.bat（会自动解除暂停），
  然后在主机窗口 Ctrl+C 重启 start_main.bat 才会重新用上它。
  注意：状态窗口要一直开着，自动暂停才会生效。

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
  开启看图（VISION=1）时主机再多约 0.9 GB，工作机不变；内存紧张的主机可以设 VISION=0。
  看图：一张图（IMAGE_TOKENS=256）在回答前多约 10~20 秒处理时间，之后生成速度不变。
  提问的机器：ask.bat 只占几十 MB；网页界面占的是浏览器内存（Supermium 一个标签页几百 MB）。
  每台机器在任务管理器 →“进程”里看“峰值工作集(内存)”：
  工作机看 ggml-rpc-server.exe，主机看 llama-server.exe。
多台机器是为了凑够内存，速度不会因为工作机多而变快（工作机越多，网络往返越多，反而略慢）。

五、常见问题
------------
- open_ui.bat / ask.bat 一直显示 Waiting / 5 分钟后报 Cannot reach：
  主机没启动、还在加载，或者主机的 8080 端口被防火墙拦截（主机上运行 setup_firewall.bat）。
- 主机扫描时工作机显示：
    not running                     那台没运行 start_worker.bat（或者登记的是以前的旧记录），正常跳过
    RUNNING but port 50052 is blocked  那台在运行，但主机连不上它的端口，几乎都是防火墙：
      1) 在那台机器上右键 setup_firewall.bat →“以管理员身份运行”；
      2) 控制面板 → Windows 防火墙 → 高级设置 → 入站规则，删掉 ggml-rpc-server 的“阻止”规则
         （第一次运行时防火墙弹窗点了“取消”就会生成这条规则，它比放行规则优先）；
      3) 主机窗口 Ctrl+C，重新运行 start_main.bat。
- 换了共享盘上的文件夹位置后，每台工作机都要在新位置重新运行 start_worker.bat，
  否则它登记在旧文件夹里，新主机看不到。
- start_worker.bat 报 Cannot write：该机器对共享盘的 cluster\workers 没有写权限。
- 集群运行期间，不要修改或覆盖共享盘上的 .bat 文件！
  cmd 是边读边执行批处理的，文件内容一变，正在运行的窗口会读到错乱的命令。
  要更新文件，先关掉所有窗口再改。
- 网页里没有上传图片的按钮 / 提示不支持图片：主机窗口 [3/3] 那行显示 images off，
  检查 model\mmproj-F16.gguf 是否存在、settings.bat 里 VISION=1，然后重启 start_main.bat。
  ask.bat / ask_think.bat 是纯文字的，不能发图片，请用 open_ui.bat。
- 中文显示成乱码：控制台字体问题，右键标题栏 → 属性 → 字体，换成 TrueType 字体。
- 安全：8080 和 50052 端口都没有密码验证，局域网内任何人都能连接。
  只在可信的机房局域网里使用，用完关闭窗口。

六、文件
--------
  start_main.bat / start_worker.bat / status.bat / open_ui.bat / ask.bat / ask_think.bat / test.bat / setup_firewall.bat
  prompt.txt                test.bat 用的提示词
  bin\                      ggml-rpc-server.exe、llama-server.exe、llama-cli.exe、qwen-helper.exe
  cluster\settings.bat      设置
  cluster\workers\          工作机自动登记（每台一个 计算机名.txt）
  cluster\server.txt        主机自动写入的 IP
  cluster\status\           工作机每 3 秒写一次的状态（给 status.bat 用）
  cluster\paused\           被自动暂停的工作机
  model\                    放模型
  src-info\                 补丁和 qwen-helper 源码

编译说明：llama.cpp dd002b9，MinGW-w64 GCC 13，_WIN32_WINNT=0x0601，静态链接，-DGGML_RPC=ON。
补丁（src-info\win7-patches.patch）：
  1) cpp-httplib：把 Win8 才有的 3 个函数换成 Win7 的等价函数；
  2) llama-cli：连接外部服务器时转发 --reasoning、--reasoning-budget、-n；
  3) 进度显示：llama-server 加载时打印进度条和剩余时间；ggml-rpc-server 打印已接收的 MB 数和速度，
     并把每次探测连接、每个缓存文件的日志降为调试级别（避免刷屏）；
  4) 工作机状态：设置 QWEN_STATUS_FILE 时，ggml-rpc-server 每 3 秒写出内存、换页速率、每步计算耗时。
  5) --reasoning-budget-message 支持 \n 转义（批处理里没法直接写换行）；
  6) 网页界面多一个 Minimal（256 token）思考档位：src-info\patch-ui.py 修改官方界面文件后
     放进 tools\ui\dist 再编译（llama.cpp 优先嵌入这个目录），未下载 npm 等任何构建工具；同一补丁把 3 个浏览器内工具设为默认允许。
  7) 思考预算用完、强制结束思考后的 8 个 token 内不允许结束回答，避免出现“只有 time is up
     提示、没有任何回答”的情况；
qwen-helper.exe（src-info\qwen-helper.c）是给批处理用的小工具：取本机 IP、探测端口、
检查服务状态、根据在线工作机数量生成分配方案。只用到 Win7 自带的系统函数。
测试：除云端 Linux 模拟外，还在 Wine（Win7 SP1 模式）里实际运行了这些 Windows 程序和批处理：
登记、自动扫描（跳过离线机器）、加载、从“共享盘”提问、工作机掉线后自动切换到 2 台方案恢复，都已验证。
