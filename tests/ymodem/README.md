# YMODEM 协议单元测试

在 Linux/macOS 上验证 YMODEM 接收端状态机, 无需硬件:

```bash
make test
```

覆盖场景:
- CRC16 校验值
- 完整传输 (1K 包 + 末包填充)
- 完整传输 (纯 128B SOH 数据包)
- 最终 ACK 丢失后的空文件头重传
- 单次 EOT 兼容变体
- 数据错误 NAK 重传恢复
- 重复包去重
- 文件头/数据包拒绝 -> CAN CAN 中止
- 发送端 CAN 中止
- "C" 周期重发
- 数据等待超时 -> 重发 ACK -> 中止
