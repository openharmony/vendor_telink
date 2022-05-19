# vendor telink

## 介绍

该仓库托管Telink开发的基于TLSR9系列SoC的演示代码，包含LED, BLE和XTS三个演示代码的配置文件和HAL模块。

## 软件目录

代码路径：

```
device/                               --- 硬件单板相关仓库
├── board/telink                      --- Telink相关单板
└── soc/telink                        --- Telink相关SoC代码

vendor/telink/                        --- vendor_telink仓库路径
├── b91_devkit_led_demo               --- LED示例代码
└── b91_devkit_ble_demo               --- BLE示例代码
└── b91_devkit_xts_demo               --- XTS示例代码
```

## 安装调试教程

参考 [安装调试教程](https://gitee.com/openharmony-sig/device_soc_telink/blob/master/README.md)

## 贡献

[如何参与](https://gitee.com/openharmony/docs/blob/HEAD/zh-cn/contribute/%E5%8F%82%E4%B8%8E%E8%B4%A1%E7%8C%AE.md)

[Commit message规范](https://gitee.com/openharmony/device_qemu/wikis/Commit%20message%E8%A7%84%E8%8C%83?sort_id=4042860)

## 相关仓

[device\_soc\_telink](https://gitee.com/openharmony-sig/device_soc_telink)

[device\_board\_telink](https://gitee.com/openharmony-sig/device_board_telink)
