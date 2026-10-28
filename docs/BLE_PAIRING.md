# Bee 蓝牙配对与恢复

## 用户操作

配对授权输入因板而异，均内部上拉、低电平有效，引脚为 3.3 V 逻辑。C3/C5
默认要求物理授权；WROOM 默认关闭，GPIO4 仅作为可选输入保留：

| 板型 | 配对 GPIO | 当前 board conf |
|------|-----------|-----------------|
| ESP32-C3 DevKitC / DevKitM / Super Mini | GPIO1 | `PAIRING_GPIO_AUTH=y` |
| ESP32-C5 DevKitC | GPIO1 | `PAIRING_GPIO_AUTH=y` |
| ESP32-WROOM-32 DevKitC | GPIO4（可选） | `PAIRING_GPIO_AUTH=n` |

C5 DevKitC 正式固件仍需在 GPIO1 外接按钮或跳线。受控台架测试可显式加载
`dev_c5_open_pairing.conf`，但该开发镜像允许附近任何主机配对，不适合正式使用；
构建命令和风险说明见[开发文档](DEVELOPMENT.zh-CN.md#配对-gpio持久化与恢复出厂)。

配对 GPIO 不被默认 UART、LED 或恢复出厂功能占用（WROOM 的 GPIO13 仅在启用
autobaud 时作按键，GPIO2 为活动 LED）。用按钮或跳线将配对 GPIO 接到 Bee 的 GND。

以下步骤适用于启用授权检查的板型（C3/C5；WROOM 需先设置
`CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH=y`）：

1. 新主机/App 首次连接前，将配对 GPIO 接地。
2. 在客户端选择 Bee 并连接，接受操作系统配对提示。无屏设备使用
   LE Secure Connections Just Works，没有需要输入的固定 PIN。
3. 配对成功后释放配对 GPIO。双方保存密钥，之后重连通过加密认证，无需操作引脚。
4. 换一台主机，重复上述步骤；原主机绑定保留。默认最多保存 8 台主机，仍只允许
   一条 BLE 连接同时工作。

启用授权时，配对 GPIO 在收到配对请求时采样；请求被拒绝后再接地不会让旧请求
继续，需要重新发起。新增绑定与替换旧密钥都受此门禁约束。引脚读取失败时拒绝
配对。低电平期间附近其他主机也可申请配对，完成后及时释放。WROOM 默认关闭
授权，因此新主机可直接配对；GATT 仍要求加密和绑定。Just Works 不提供 MITM
身份核验。

## 客户端

- Web / iOS：读取加密的 Management Protocol 特征时由系统处理配对。
  浏览器选择设备的授权不等于蓝牙绑定。如果系统未弹出配对提示，可先在系统
  蓝牙界面配对，再重连；不同浏览器/操作系统组合需实机验证。
- Android App：连接后检查系统绑定状态；缺少绑定时调用 `createBond`，失败则
  断开连接，不继续读写串口或管理特征。
- Python CLI：`--pair` 在 Linux 等支持的平台主动调用系统配对；macOS 在读取
  加密特征时弹出系统配对。首次操作时，启用授权检查的板型先拉低配对 GPIO
  （C3/C5 为 GPIO1；WROOM 默认跳过），必要时增加 `--timeout`。
  连接或配对失败以退出码 `1` 结束（终端会话中设备断开是 `3`）；先 `--scan`
  可以确认设备是否在场（它会列出全部附近设备，包括未广播名字的）。
- Linux C CLI：`--pair` 调用 BlueZ `Device1.Pair`。系统需有可用的 Bluetooth
  agent；已有绑定不删除、不强制替换。也可先通过 `bluetoothctl` 配对。
  退出码与 Python 客户端一致：`1` 连接/配对失败，`2` 参数问题，`3` 终端会话中
  设备断开；`--scan` 可先确认设备是否在场（列出全部附近设备，含无名的）。
- HarmonyOS：建立 GATT 连接后检查系统绑定状态；未绑定时主动调用 `pairDevice`
  并等待绑定成功，再协商 MTU、发现服务及访问加密特征。正在系统配对时不重复
  发起请求。首次连接前先拉低配对 GPIO，并接受系统配对提示；失败或 60 秒内
  未完成则断开，不继续访问服务。已绑定主机跳过配对，不删除已有绑定。
  自动配对弹窗及恢复流程仍需真机验证。

## 绑定丢失、存储与恢复

- Bee 使用 Zephyr `BT_SETTINGS` 在 NVS 中保存绑定密钥，并在开始广播前恢复。
- 主机端“忽略设备”不会删除 Bee 端记录；主机密钥丢失后，启用授权检查的板型
  重新配对仍需拉低配对 GPIO，WROOM 默认不需要。
- 默认 `BT_MAX_PAIRED=8`，`BT_KEYS_OVERWRITE_OLDEST=n`，满时不自动移除旧主机。
- 恢复出厂会同时清除全部绑定、BLE identity 和其他保存的设置：C3 在上电时
  持续拉低 GPIO0 至少 2 秒；C5 为 GPIO28，WROOM 为 GPIO27。C3 Super Mini 的
  BOOT 引脚不是 GPIO0，请按板级接线操作。恢复出厂后主机也应忽略旧记录；
  启用授权检查的板型再拉低配对 GPIO 重新配对，WROOM 默认直接重连。
- 配对 GPIO 仅授权配对，长期拉低不会清空已有设置。本版不提供无线删除单个绑定接口。

## 固件保证与范围

Management、Reliable UART 和兼容 NUS 的特征值及 CCC 均要求加密访问；
未配对连接可以发现服务，但不能收发串口数据或修改管理配置。NUS 使用本项目
定义的加密实例，上游默认明文实例已禁用。新主机必须支持 LE Secure Connections
及 16 字节密钥；不允许不保存绑定的临时配对。

蓝牙门禁不改变局域网 WebSocket 的网络/token 策略，也不提供 Flash 静态数据加密。

## 广播发射功率

固件在蓝牙栈启用后显式设置发射功率，不再沿用控制器自带默认值：
`CONFIG_LINKR_BLE_BRIDGE_BLE_TX_POWER_DBM`（默认 **9 dBm**）。设置的是控制器的
default 功率桶，广播与新建连接都继承它。

这条是为了“看得见”，不是为省电。同一位置、同一时间窗内的四档对照（Windows 用
`BleakScanner` 各扫 3 轮 × 12 s，平板用 nRF Connect 连续扫描）：

| 功率 | Windows | 平板 | 峰值电流（WROOM-1 数据表） |
|---|---|---|---|
| 控制器默认（未设置） | 轮2 完全扫不到；轮1/3 为 -80/-79 dBm 且**没有名字** | -86 dBm | -1 dBm 档 134 mA |
| **+9 dBm**（默认） | 0/3，不报告该设备 | -81 dBm | ~175 mA（8 dBm 档） |
| +15 dBm | 3/3，-76 ~ -80 dBm，带名字 | -76 dBm | ~250 mA（15 dBm 档） |
| +20 dBm | 4/4，-71 ~ -77 dBm，带名字 | -64 ~ -72 dBm | 319 mA（19 dBm 档） |

> 这张表**不是**一次干净的固件对照：把板子挪一下再放回原位，同一测量点的读数
> 就会变约 7 dB，所以档位之间的大部分差异来自环境（多径、主机 WiFi/BT 共存、
> 人体吸收），而不是功率设置本身。它能说明的只有一件事：**显式设置功率**比沿用
> 控制器默认（-86 dBm）好得多。

**抖动主要来自接收主机，不是发射端**。同一个 9 dBm 镜像、板子没动，隔一段时间
重测的结果：

| 时刻 | Windows | 平板 |
|---|---|---|
| 第一次 | 0/3，扫不到 | -81 dBm |
| 重测（同一镜像） | **3/3，-60 ~ -67 dBm，带名字** | **-79 dBm（与第一次一致）** |

平板读数稳定（9 dBm 约 -79、15 dBm 约 -76、20 dBm 约 -64，差值符合功率差），
说明发射链路是稳定可控的；变化 15-20 dB 的是 Windows 一侧——Intel 网卡的 WiFi/BT
共存、扫描占空比和驱动状态都会影响它。所以排查顺序应该是：先看主机（关 WiFi 再测、
换 USB 口、换主机），再看现场（位置、朝向、遮挡），最后才动发射功率。

9 dBm 在这种主机上属于"余量偏薄"：主机状态好时 -60 dBm 很宽裕，状态差时直接扫
不到。要更省心的主机兼容性就用 15 dBm。

`main` 的纯 SRAM 镜像（未设置功率）上同样复现，说明这是既有缺陷，不是 PSRAM
引入的回归。默认档下“扫到也没名字”是关键细节：名字只在 scan response 里，弱信号
时 response 先丢，而 Windows 的系统蓝牙列表对无名设备更不友好。

**为什么出厂用 9 dBm**：9 dBm 加 2-3 dBi 天线增益约 **11-12 dBm EIRP**，离 CE 与
中国 SRRC 的 20 dBm EIRP 限值很远，峰值电流约 175 mA（19 dBm 档是 319 mA）。
这是"显式设置、别用控制器默认"这件事的最小代价取值。

**现场扫不到时怎么办**：把 `CONFIG_LINKR_BLE_BRIDGE_BLE_TX_POWER_DBM` 调到
**15 dBm**（EIRP 约 17-18 dBm，仍在限值内），再不行才考虑 20 dBm（EIRP 约
22-23 dBm，已超 CE/SRRC 限值）。这样排序是因为在测试机上 9 dBm 曾 0/3 扫不到、
15 dBm 同一主机 3/3，但那次差异里也混着 ~7 dB 的环境变化，不能证明 9 dBm 本身
不够。先排除位置、遮挡、主机 WiFi 占用，再动功率。

广播档（`BT_HCI_VS_LL_HANDLE_TYPE_ADV`）无法在 `bt_le_adv_start()` 之前单独设置
（此时没有广播实例，控制器返回 `EINVAL`），所以固件只设 scan 与 default 两档。
启动日志会打印控制器实际选中的档位，夹紧时直接可见：

```
<inf> linkr_ble_bridge: BLE TX power: type 1 requested 9 dBm, selected 9 dBm
<inf> linkr_ble_bridge: BLE TX power: type 255 requested 9 dBm, selected 9 dBm
```

控制器上限：C5 为 20 dBm，C3 为 9 dBm（更高的请求被映射到该芯片最高档，所以
C3 上这个默认值即其最高档）。改用电池供电的变体时，这个值应作为第一个重新
评估的项。


实现使用 Zephyr 的同步 `pairing_accept` 回调在收到请求时决定是否继续，接口说明见
[Zephyr authentication callbacks](https://docs.zephyrproject.org/latest/doxygen/html/structbt__conn__auth__cb.html)。

## Windows 主机实测（2026-10-28，C5 + `dev_c5_open_pairing.conf`）

Windows 11 + Intel 无线网卡，`bleak` 3.0.2 / `tools/linkr_ble_terminal.py`，9 dBm 镜像：

| 项目 | 结果 |
|---|---|
| 扫描发现 | -61 ~ -66 dBm，12 次采样 0 丢失，标准差 1.8 dB |
| 配对 | 正式镜像要求 GPIO1 接地（`Insufficient Authentication`）；bench 镜像 `BleakClient.pair()` 直接成功 |
| GATT | GAP、GATT、Linkr Management（`4c4b0001-…`）、NUS（`6e400001-…`）、Reliable UART（`4c4b0010-…`） |
| 管理通道 | `@n?`/`@i?`/`@u?`/`@w?`/`@c?` 全通，`security=2`，`@c?` 报 `peers=1 ble_max=2` |
| 串口桥 | 写 `echo probe-ok\r\n` → 收到 111 B 带 bracketed-paste 的 radxa shell 回显 |
| 大流量 | `ls -R … | head -2200` → 40134 B / 8.1 s，`uart/queue/peer dropped` 全 0 |
| 断连重连 | 连续 3 次，每次 MTU 247，绑定生效、无需重新配对 |
| 长连接 | 85 s / 76 次心跳全通，0 断连 |
| WiFi 扫描（BLE 保持连接） | `@w scan` 扫到 10 个 AP，BLE 链路未断 |
| WiFi 配网（BLE 保持连接） | `@w=b601_a` → `phase=ready ip=192.168.31.99`，BLE 全程不断 |
| 改名 | `@n=Linkr Bee T1` 立即出现在广播、`@n?` 报 `custom=yes`；`@n default` 恢复 |

两个 Windows 侧注意事项（都不是固件问题）：

- `WinError -2147023673`（"操作已被用户取消"）偶发率约 1/2，重试即可，见下节。
- `tools/linkr_ble_terminal.py --timeout N` 会被传给 `BleakClient(timeout=N)`。给
  `--wifi-scan` 顺手加 `--timeout 55` 会让连接在管理通道建立前被取消，看起来像
  WiFi 扫描把 BLE 打掉了，实际是参数用错。扫 WiFi 不要动 `--timeout`。

多主机共存也测过了（Windows + Android 平板上的 Linkr Bee App 同时连接）：

```
[win] t=9s  @c? -> self=10 owner=10 mode=control epoch=10 peers=1 ble_max=2
[win] t=27s @c? -> self=10 owner=10 mode=control epoch=10 peers=2 ble_max=2   ← 平板接入
[win] t=46s @c? -> peers=2
[win] t=64s @c? -> peers=2
[win] t=83s @c? -> peers=2
```

Windows 侧五轮采样里心跳全部正常、没有掉线，控制权始终在 Windows 手上；平板
App 同时把 `@i?`/`@w?`/`@s?`/`@n?` 都查了一遍（`security=2`），并且能看到
UART 侧输出（`radxa@radxa-cubie-a5e:~$`）。也就是说 `BT_MAX_CONN=2` 的两条链路
和"只允许一个控制者"的仲裁都是按设计工作的。

平板侧的操作提示：nRF Connect 的 `CONNECT` 按钮对 `adb shell input tap` 注入的
点击不响应（扫描中点到的是别的设备详情），用平板上的 Linkr Bee App 点「重新连接」
（顶部绿色按钮，中心约 `(1017, 179)`）最省事。

## 实机验收（构建与主机测试不能替代）

下表针对启用配对授权的配置（C3/C5，或手工开启的 WROOM）。WROOM 默认
`PAIRING_GPIO_AUTH=n`，应预期任何新主机都不需要 GPIO4 即可配对。

| 步骤 | 预期 |
| --- | --- |
| 清空测试主机绑定，配对 GPIO 悬空/高，尝试配对及 GATT 读写/订阅 | 拒绝配对；三个服务不泄露数据 |
| 配对 GPIO 拉低，主机 A 配对 | 成功并可使用 UART/管理服务 |
| 释放配对 GPIO，断开并重连 A | 复用密钥成功 |
| Bee 断电重启，再连接 A，配对 GPIO 高 | 绑定恢复，仍可使用 |
| 配对 GPIO 高，主机 B 申请配对 | 拒绝，A 绑定仍可用 |
| 配对 GPIO 低，B 配对，然后释放 | A、B 均可分别重连 |
| A 忽略绑定，配对 GPIO 高/低分别重配 | 高拒绝、低允许替换密钥 |
| 绑定表满后新增主机 | 拒绝，不淘汰原绑定 |
| 普通固件更新不擦设置区后重连 | 原绑定继续有效 |
| 执行板级恢复出厂 | 全部绑定清空，配对 GPIO 高时旧主机不能重新配对 |

表中“配对 GPIO”在 C3/C5 为 GPIO1；WROOM 默认关闭授权，手工启用时才是 GPIO4。
建议覆盖 C3/C5/WROOM、Android、iOS、桌面 Chrome 与 Linux BlueZ，并在配对
期间并发 UART 流量及 WiFi 活动验证栈和内存余量。
