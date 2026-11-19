# AssetForge17 · 可复现的本地资源包构建器

给定一份带依赖和构建配置的资源清单，AssetForge17 找齐所需资源，将它们按固定顺序写入 AFP 包。CLI 可以预览构建计划、按 ID 或别名查询包内条目，也可以校验每个数据块。

课程对照和线格式见 [工程设计](DESIGN.md)，源码阅读与实验见 [学习指南](STUDY_GUIDE.md)。项目使用 C++17、CMake 和 CTest。

## 第一次运行

在项目目录下用 PowerShell 构建并执行示例。

~~~powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure

.\build\assetforge.exe plan data/demo.assets --profile demo
.\build\assetforge.exe build data/demo.assets --profile demo --out out/demo.afp --report normalization
.\build\assetforge.exe inspect out/demo.afp --id startup --detail
.\build\assetforge.exe verify out/demo.afp --policy par
~~~

WSL 中可使用默认生成器构建，并通过 `./build-linux/assetforge` 运行。

~~~sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux -j 4
ctest --test-dir build-linux --output-on-failure
./build-linux/assetforge plan data/demo.assets --profile demo
~~~

`demo` 配置选中 `ui.start` 和它依赖的 `ui.logo`、`ui.theme`。写入顺序为 Logo、主题、启动配置，所以相对内容区的偏移分别是 0、3、15。两个文本文件各有一对 CRLF，打包时转换为 LF。

~~~text
BUILT assets=3 aliases=1 bytes=26
normalized_assets=2 crlf_pairs=2
~~~

## 使用自己的清单

字段之间使用一个真实 Tab。可复制 [demo.assets](data/demo.assets) 改写。路径以清单所在目录为基准，使用正斜杠，文件名可以包含空格。`asset` 定义 ID、处理模式、路径、可选大小上限。`requires` 指明依赖，`alias` 起公开别名，`profile` 给出逗号分隔的根 ID。`--root` 可直接选一个 ID 或别名。

- `raw` 保留原始字节。`text-lf` 将 CRLF 转为 LF，遇到裸 CR 或 NUL 时构建失败。数字大小上限按处理后字节数计算，`-` 采用默认上限。
- `--out` 指定首次发布的目标文件，构建期间应独占输出路径，并保持输入稳定。已有成品需要选另一个文件名。程序先写临时文件，验证通过后发布。
- `--policy seq` 和 `--policy par` 分别选择顺序及并行策略。并行阶段只计算 CRC，实际调度取决于标准库和后端。CMake 检测到 TBB 时会链接对应目标。
- 退出码 0、1、2 分别表示成功，业务或 I/O 失败，以及命令用法错误。`inspect` 读取并检查索引，`verify` 还会核对所有资源的内容 CRC。

## 交互工作区

~~~powershell
.\build\assetforge.exe shell data/demo.assets --profile demo
~~~

~~~text
plan
rename-alias startup launch
build --out out/renamed.afp --report sizes
use minimal
plan
reload
quit
~~~

改名后生成的包可通过 `inspect out/renamed.afp --id launch --detail` 查看。`reload [MANIFEST]` 加载候选清单并保留当前配置。如果候选内找不到当前配置，会话可用原目录继续运行。文件名含空格时使用双引号，路径分隔使用 `/`。会话可以通过标准输入执行 UTF-8 脚本，任一指令失败时最终退出码为 1。

库用户可以调用 `load_catalog`、`plan`、`build` 和 `BundleReader::open`。`Workspace::snapshot` 持有自己的目录和选择，可在另一线程规划。`BundleReader` 独占文件流与游标，`read` 验证并返回拥有字节的值。
