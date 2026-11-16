# 烧录指南（Arduino IDE 2.x 详细版）

适用于本仓库固件：

| 固件 | 工程目录 | 说明 |
| --- | --- | --- |
| **BW16 Deauth（仅去认证）** | `firmware/deauthonly/deauthonly.ino` | 扫描 + Deauth + 停止 + MAC 定向 + 热点密码修改 |

开发板：**BW16（Ai-Thinker / RTL8720DN，双频）**

---

## 0. 需要准备

| 项目 | 要求 |
| --- | --- |
| 开发板 | BW16（RTL8720DN）开发板 |
| USB 线 | **能传数据**的线（很多线只有供电，插上只能充电——这是"电脑没识别到板子"的头号原因） |
| 电脑 | Windows 10/11、macOS 或 Linux |
| 软件 | **Arduino IDE 2.x**（推荐 2.2 以上） |

> 不需要单独装 USB 驱动：BW16 通过自身 USB 串口通信，Windows 10/11 与 macOS 免驱。

---

## 1. 安装 Arduino IDE

到 <https://www.arduino.cc/en/software> 下载 **Arduino IDE 2.x** 安装包，安装后打开。

---

## 2. 添加 Realtek AmebaD 开发板支持

1. 菜单 **文件 → 首选项**（macOS：**Arduino IDE → Settings**）
2. 找到 **附加开发板管理器网址 / Additional boards manager URLs**，点右侧图标填入下面这一行（多个网址用逗号分隔，只有一个就直接粘贴）：

   ```
   https://raw.githubusercontent.com/Ameba-AIoT/ameba-arduino-d/master/Arduino_package/package_realtek_amebad_index.json
   ```

   点 OK 保存。
3. 菜单 **工具 → 开发板 → 开发板管理器**，搜索框输入 `AmebaD`
4. 找到 **Realtek AmebaD Boards**，选择版本 **3.1.9**，点 **安装**（约 700 MB，需耐心等待）

> 本项目所有编译验证都基于 **3.1.9**。升级到更高版本可能因核心 API 变化导致编译失败——建议锁定 3.1.9。

---

## 3. 安装依赖库

1. 左侧栏点 **库管理器**（书本图标）
2. 搜索 `ArduinoJson`
3. 选择 **ArduinoJson 7.0.4**（必须 v7；v6 与固件代码不兼容）→ 安装

> `WiFi`、`AmebaFlashMemory` 由开发板核心自带，不用单独安装。

---

## 4. 打开工程

菜单 **文件 → 打开**，选择 `firmware/deauthonly/deauthonly.ino`。

⚠️ **必须打开对应目录里的 .ino 文件**：同目录的 `ui.h`/`webui.h`/`capture_pages.h`/`wifi_tx.*` 会被自动一并编译；只把 .ino 单独复制到别处会编译失败。

---

## 5. 选择开发板与参数（关键一步）

1. **工具 → 开发板 → Realtek AmebaD → Ai-Thinker BW16 (RTL8720DN)**
2. **工具 → 端口**：选择新出现的 COM 口（插拔板子时列表会多出一项）
3. **工具 → Auto Upload Mode → Enable** ← **必选**，否则上传时会报 `Enter Uart Download Mode` 失败
4. **工具 → Erase Flash → Disable**
   - 需要**恢复出厂/清除已保存的热点密码**时改成 **Enable**（会整片擦除 Flash，设置与密码回到默认）

设置好的工具栏应类似：

```
开发板: "Ai-Thinker BW16 (RTL8720DN)"
端口:   COMx
Auto Upload Mode: Enable
Erase Flash: Disable
```

---

## 6. 编译并上传

1. 点左上角 **→（上传）** 按钮，或按 **Ctrl+U**
2. IDE 会先编译（首次约 1–2 分钟），随后自动完成：复位板子 → 进入下载模式 → 写入 → 重启
3. 成功标志（查看下方输出窗口）：

   ```
   All images are sent successfully!
   New upload port: COMx (serial)
   ```

> 上传期间不要操作板子或拔线。整个过程约 20–40 秒。

---

## 7. 第一次使用

1. 手机/电脑连热点：
   - SSID：**`BW16-Deauther`**
   - 密码：**`0123456789`**（精简版默认；全功能版如果改过，用你改后的密码）
2. 浏览器打开 **<http://192.168.1.1>** → 出现管理页面
3. 点「重新扫描」→ 勾选目标 → 设帧类型/间隔 → 「启动 Deauth」

LED 状态（精简版）：**空闲绿色 / 攻击 2.4G 红色 / 攻击 5G 蓝色**。

---

## 8. 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 端口列表里看不到板子 | 充电线、USB 口坏、线接触不良 | 换一根数据线、换 USB 口（优先直连主板后置口，别用扩展坞） |
| 上传报 `Enter Uart Download Mode` / `Failed to open COMx` | 没开自动下载模式，或端口被占用 | 板级菜单 **Auto Upload Mode = Enable**；关闭串口监视器、VS Code 串口、其他 IDE 实例；再点上传 |
| 上传中途失败/超时 | 线材或供电不足 | 换线、换口、拔掉其他 USB 设备重试 |
| 上传成功但热点不出现 | 启动异常或选错板子 | 断电重插等 10 秒；确认开发板选的是 **Ai-Thinker BW16**；重烧时 `Erase Flash = Enable` |
| 手机打不开 `192.168.1.1` | 手机还连着家里 Wi-Fi 或移动数据抢路由 | 关闭移动数据；确认 Wi-Fi 名称是 `BW16-Deauther`；地址栏输入完整的 `http://192.168.1.1` |
| 串口监视器一片空白 | 板子的 LOG 串口不接在 USB 口上，属正常现象 | 调试请看网页诊断端点 `/api/dbg`（精简版）或事件日志 |
| 忘记热点密码 | 密码保存在 Flash（`0x00100000`） | 重新烧录时 `Erase Flash = Enable`，密码恢复默认 `0123456789` |
| 扫描时热点断开几秒 | 单射频模块无法边扫描边广播 | 正常现象，扫描结束自动恢复，手机重连即可 |

---

## 9. 用命令行烧录（可选，arduino-cli）

不想装 IDE，或需要自动化时：

```bash
# 1) 安装核心与库
arduino-cli core install realtek:AmebaD@3.1.9
arduino-cli lib install "ArduinoJson@7.0.4"

# 2) 编译
arduino-cli compile --fqbn realtek:AmebaD:Ai-Thinker_BW16 firmware/deauthonly

# 3) 烧录（AutoUploadMode=Enable 让工具自动复位板子，必须带）
arduino-cli upload -p COMx --fqbn realtek:AmebaD:Ai-Thinker_BW16:AutoUploadMode=Enable firmware/deauthonly
```

> 仓库里的 `arduino-cli.yaml` 是便携配置（`data.dir` 指向 `./.arduino`），本机已装好工具链时可用
> `arduino-cli --config-file arduino-cli.yaml ...` 直接复用，不必重新下载核心。

### 直接用 release 里的 bin 烧录

`release/*.bin` 是 `km0+km4` 合并镜像（下载地址 `0x0000.0000`），可用 **Realtek Ameba 烧录工具** 选择 image2、起始地址 `0x00000000` 写入。

- 若工具提示格式校验失败，请改用上面的 Arduino / arduino-cli 方式；
- 镜像不是"给芯片的裸 bin"，而是给 Realtek 工具的 image2 格式，不要用通用 ESP/ST-Link 工具乱写。

---

## 10. 注意事项

- 固件默认热点密码 `0123456789` 是公开的；改过密码请务必记牢（忘记只能擦除 Flash 重烧）。
- 烧录会**覆盖**板子原有固件，请确认板子是自己的、可以刷。
- 本项目仅用于自有网络的设备与安全测试，请勿用于未授权的网络。
