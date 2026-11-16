# BW16 Deauth 固件（仅去认证攻击）

面向 **BW16（Ai-Thinker / RTL8720DN 双频）** 开发板的 Wi-Fi 扫描 + Deauth 攻击固件，**全部操作在浏览器网页完成**，无需串口。

> 当前发布版只包含 **扫描 + Deauth 攻击**。Evil Portal 钓鱼、克隆等全功能版本仍在开发验证中，**尚未验证、暂不发布**。

- 固件版本：`v1.0.5` ｜ 编译日期：2026-11-16
- 发布件：`release/deauthonly_bw16_v1.0.5.bin`（源码：`release/deauthonly_src_v1.0.5.zip`）
  - km0 + km4 合并镜像（image2），下载地址 **0x0000.0000**
  - 大小 778,240 字节（占 Flash 37%）
  - SHA-256：`fe53479ca9ebba6bc4e16c8d5de42e6934d02e94c313a1dda76a51539cb99d19`
- 默认热点：`BW16-Deauther` / `0123456789` ｜ 管理页：<http://192.168.1.1/>

> 📖 **不会烧录？看详细步骤：[FLASH_GUIDE.md](FLASH_GUIDE.md)**（Arduino IDE 2.x 从安装开发板到上传、排错全覆盖）。

## 功能

| 模块 | 说明 |
| --- | --- |
| 扫描 | 2.4 GHz + 5 GHz，按信号强度保留最强 16 个网络（信道/信号/加密/MAC） |
| 目标选择 | 点行多选，最多 16 个 BSSID 同时攻击 |
| Deauth 引擎 | Deauth / Disassoc / 组合三种帧；间隔 100 / 200 / 500 / 1000 ms（默认 100） |
| 持续嗅探 + 单播打击 | 攻击期间嗅探常开，自动捕获目标网络客户端 MAC 并定向单播 Deauth；**随机 MAC 设备也能自动跟打**，无需手动填 MAC |
| 手动 MAC（可选） | 也可填「要做掉的设备」的 MAC 定向攻击；本热点在线设备可一键填入 |
| 平滑换信道 | 目标在单一信道时，用 CSA 通知把热点平滑切到目标信道（**不重建 AP**，客户端自动跟随），停止后切回信道 1 |
| 状态灯 | 板载 RGB：空闲绿色 / 攻击 2.4G 红色 / 攻击 5G 蓝色（页面有图例） |
| 热点密码 | 网页可修改热点密码，写入 Flash 持久化 |
| 诊断 | 事件日志 + `/api/dbg`（帧数/发送失败/嗅探帧数/捕获客户端/信道状态等） |

## 快速上手

1. **上电**：手机连热点 `BW16-Deauther`（密码 `0123456789`）
2. 浏览器打开 **<http://192.168.1.1>**
3. 点 **重新扫描**，等待几秒（期间热点会断开约 5 秒，属正常，结束后自动恢复）
4. 勾选目标网络 → 选帧类型与间隔 → **启动 Deauth**（MAC 框留空即自动嗅探）
5. 观察目标设备是否掉线；随时点 **停止**（热点回到原信道）

## 从源码构建

仓库只含源码与发布件，`.arduino/`、`.tools/`、`.ref/` 是本地构建环境（已在 `.gitignore` 排除）。

```bash
# 1) 安装核心与库
arduino-cli core install realtek:AmebaD@3.1.9
arduino-cli lib install "ArduinoJson@7.0.4"

# 2) 编译
arduino-cli compile --fqbn realtek:AmebaD:Ai-Thinker_BW16 firmware/deauthonly

# 3) 烧录（AutoUploadMode=Enable 让工具自动复位板子，必须带）
arduino-cli upload -p COMx --fqbn realtek:AmebaD:Ai-Thinker_BW16:AutoUploadMode=Enable firmware/deauthonly
```

也可用 Arduino IDE 2.x 直接打开 `firmware/deauthonly/deauthonly.ino` 上传，参数见 [FLASH_GUIDE.md](FLASH_GUIDE.md)。

## 目录结构

```
firmware/deauthonly/
  deauthonly.ino   主程序：AP/扫描/攻击引擎(持续嗅探+单播)/MiniHttp/Flash 密码
  ui.h             单页 Web 界面
  wifi_tx.h/.cpp   802.11 管理帧注入（Deauth 0xC0 / Disassoc 0xA0）
release/           预编译固件 + 源码包
FLASH_GUIDE.md     烧录详细指南
NOTICE.md          第三方致谢与免责声明
```

## 主要接口

`GET /api/status`（状态/事件/网络）、`GET|POST /api/scan`（异步扫描）、`POST /api/attack/start|stop`、`POST /api/setpass`（改热点密码）、`GET /api/dbg`（诊断）。

## 已知边界

- **单射频**：扫描、切换频段时热点会中断约 5 秒（页面有提示），结束后自动恢复。
- **广播 vs 单播**：广播 Deauth 只能造成短暂掉线；本固件默认用嗅探到的客户端 MAC 做**单播**定向，才能稳定持续阻断。
- **随机 MAC**：手机对每个网络会用不同随机 MAC，固件在攻击期间持续嗅探并滚动更新 MAC 表（表满淘汰最久未见项）。
- **802.11w / PMF**：强制 PMF 的路由器会丢弃未受保护的 Deauth，表现为踢不动——需在路由器关闭 PMF 或改用 WPA2。
- **Mesh / 多 AP**：客户端会漫游重连，测试建议用单台独立路由器。
- 事件日志与抓取信息只在 RAM；热点密码存 Flash `0x00100000`（烧录时 `Erase Flash = Enable` 可恢复默认密码）。

## 故障排查

| 现象 | 处理 |
| --- | --- |
| 打不开 `192.168.1.1` | 关闭手机移动数据；确认连的是 `BW16-Deauther`；地址输入完整 `http://192.168.1.1` |
| 扫描后热点消失 | 正常：扫描会中断热点约 5 秒；若持续不恢复，断电重插 |
| 启动攻击后热点不见 | 固件会用 CSA 把热点切到目标信道，手机重新发现并连回；若仍异常，停止攻击即回原信道 |
| 目标踢不掉 | 确认目标与 BW16 距离 2–5 米；路由器若开 PMF/WPA3 需关闭；用 `/api/dbg` 看 `frames/txfail/cliCnt` |
| 上传失败 | 见 [FLASH_GUIDE.md](FLASH_GUIDE.md) 排错表（多为线材或 `Auto Upload Mode` 未开） |

## 致谢与免责

第三方组件与授权、免责声明见 [NOTICE.md](NOTICE.md)。本固件仅用于自有网络的安全测试与学习。
