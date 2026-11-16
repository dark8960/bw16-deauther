# 说明与第三方致谢

本仓库发布的是 BW16（RTL8720DN）**仅去认证攻击（Deauth）**固件：`firmware/deauthonly`。

Evil Portal 钓鱼、克隆等全功能版本仍在开发验证中，**尚未验证、暂不发布**（仅本地开发，不在仓库内）。

## 第三方组件与授权

| 组件 | 用途 | 授权/来源 |
| --- | --- | --- |
| Realtek AmebaD Arduino 核心 3.1.9 | 芯片底层、WiFi、FlashMemory | Realtek SDK 许可（随核心分发） |
| ArduinoJson 7.0.4 | Web/JSON 序列化 | MIT |
| `wifi_tx.cpp` 管理帧注入实现 | 参照 RTL8720dn-Deauther（tesa-klebeband）与 cypher / cypher-5G-deauther 的公开实现改写 | 遵循上述项目原始授权 |

固件与 Web UI 的其余代码为本仓库作者编写。

## 免责声明

本项目仅供自有网络的**安全测试与学习**使用。请勿对未授权的网络或设备使用；由此产生的一切后果由使用者自行承担。
