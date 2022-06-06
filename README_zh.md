# vendor telink


-   [介绍](#介绍)
-   [软件目录](#软件目录)
-   [安装调试教程](#安装调试教程)
-   [贡献](#贡献)
-   [相关仓库](#相关仓库)

## 介绍

该仓库托管Telink开发的基于TLSR9系列SoC芯片的演示代码，包含LED, BLE和XTS三个演示样例的代码的配置文件和HAL模块。

## 软件目录

和TLSR9系列SoC芯片的演示样例相关的代码文件路径如下：

```bash
.
└── device                          # 硬件单板相关仓库
    ├── board
    .   ├── ...
    │   └── telink                      # Telink相关单板  
    ├── ...
    └── soc
        ├── ...
        └── telink                      # Telink相关SoC代码

.
└── vendor
    └── telink                          # vendor_telink仓库路径
        ├── b91_devkit_ble_demo             # BLE示例代码
        ├── b91_devkit_led_demo             # LED示例代码
        ├── b91_devkit_xts_demo             # XTS示例代码
        ├── LICENSE                         # 证书文件
        ├── OAT.xml                         # 开源仓审查规则配置文件
        ├── README.md                       # 英文版README
        └── README_zh.md                    # 中文版README  
```

全面而详细的目录结构，包括演示样例的代码的配置文件和HAL模块所在位置，可以参考TLSR9系列SoC芯片SDK仓库README文档中的章节[确认目录结构](https://gitee.com/openharmony-sig/device_soc_telink/blob/master/README_zh.md#确认目录结构)。

## 安装调试教程

在配置完成OpenHarmony的开发环境，获取源码之后，选择和编译样例工程，可以生成以二进制形式保存的设备固件。

使用指定的烧录工具，将固件烧录进TLSR9系列SoC芯片的开发板（如B91）后，重新上电即可运行示例并开始进行调试。

详情可以参考TLSR9系列SoC芯片SDK仓库中的[README](https://gitee.com/openharmony-sig/device_soc_telink/blob/master/README_zh.md)文档。

## 贡献

[如何参与](https://gitee.com/openharmony/docs/blob/HEAD/zh-cn/contribute/%E5%8F%82%E4%B8%8E%E8%B4%A1%E7%8C%AE.md)

[Commit message规范](https://gitee.com/openharmony/device_qemu/wikis/Commit%20message%E8%A7%84%E8%8C%83?sort_id=4042860)

## 相关仓库

TLSR9系列SoC芯片SDK：[device\_soc\_telink](https://gitee.com/openharmony-sig/device_soc_telink)

B91开发板：[device\_board\_telink](https://gitee.com/openharmony-sig/device_board_telink)
