<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<!--
  Battery HMI — Chinese (Simplified) translation
  Generated: 2026-07-02
  To compile: lrelease battery_hmi_zh_CN.ts -qm battery_hmi_zh_CN.qm
  Or use: python3 scripts/ts2qm.py translations/battery_hmi_zh_CN.ts
-->

<!-- ═══ StatusPanel ═══ -->
<context>
    <name>StatusPanel</name>
    <message>
        <source>Battery Status</source>
        <translation>电池状态</translation>
    </message>
    <message>
        <source>SOH</source>
        <translation>健康度</translation>
    </message>
    <message>
        <source>Stage</source>
        <translation>阶段</translation>
    </message>
    <message>
        <source>RUL</source>
        <translation>剩余寿命</translation>
    </message>
    <message>
        <source>Temp</source>
        <translation>温度</translation>
    </message>
    <message>
        <source>Cycle</source>
        <translation>循环</translation>
    </message>
    <message>
        <source>Capacity</source>
        <translation>容量</translation>
    </message>
    <message>
        <source>Pressure</source>
        <translation>压力</translation>
    </message>
    <message>
        <source>HEALTHY</source>
        <translation>健康</translation>
    </message>
    <message>
        <source>DEGRADING</source>
        <translation>退化</translation>
    </message>
    <message>
        <source>EOL</source>
        <translation>寿命终止</translation>
    </message>
    <message>
        <source>Battery in good condition</source>
        <translation>电池状态良好</translation>
    </message>
    <message>
        <source>Performance declining</source>
        <translation>性能衰减中</translation>
    </message>
    <message>
        <source>End of life reached</source>
        <translation>已达寿命终点</translation>
    </message>
    <message>
        <source>UNKNOWN</source>
        <translation>未知</translation>
    </message>
    <message>
        <source>No assessment yet</source>
        <translation>暂无评估</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>正常</translation>
    </message>
    <message>
        <source>Stabilizing… %1/%2 samples</source>
        <translation>稳定中… %1/%2 样本</translation>
    </message>
</context>

<!-- ═══ MainWindow ═══ -->
<context>
    <name>MainWindow</name>
    <message>
        <source>🔋 BATTERY HMI v1.0</source>
        <translation>🔋 电池健康评估 HMI v1.0</translation>
    </message>
    <message>
        <source>SOH:</source>
        <translation>健康度:</translation>
    </message>
    <message>
        <source>SOH: %1%</source>
        <translation>健康度: %1%</translation>
    </message>
    <message>
        <source>SOH: %1% ±%2%</source>
        <translation>健康度: %1% ±%2%</translation>
    </message>
    <message>
        <source>SOH: %1% (⏳%2/%3)</source>
        <translation>健康度: %1% (⏳%2/%3)</translation>
    </message>
    <message>
        <source>HEALTHY</source>
        <translation>健康</translation>
    </message>
    <message>
        <source>DEGRADING</source>
        <translation>退化</translation>
    </message>
    <message>
        <source>EOL</source>
        <translation>寿命终止</translation>
    </message>
    <message>
        <source>CNN Error: %1</source>
        <translation>CNN 错误: %1</translation>
    </message>
    <message>
        <source>Data: %1</source>
        <translation>数据源: %1</translation>
    </message>
</context>

<!-- ═══ ChartWidget ═══ -->
<context>
    <name>ChartWidget</name>
    <message>
        <source>Voltage (V)</source>
        <translation>电压 (V)</translation>
    </message>
    <message>
        <source>Current (A)</source>
        <translation>电流 (A)</translation>
    </message>
    <message>
        <source>Charge / Discharge Curves</source>
        <translation>充放电曲线</translation>
    </message>
    <message>
        <source>Time (s)</source>
        <translation>时间 (秒)</translation>
    </message>
</context>

<!-- ═══ CycleProgressBar ═══ -->
<context>
    <name>CycleProgressBar</name>
    <message>
        <source>Cycle Test Progress</source>
        <translation>循环测试进度</translation>
    </message>
    <message>
        <source>%v / %m cycles</source>
        <translation>%v / %m 次循环</translation>
    </message>
    <message>
        <source>Elapsed: --:--:--</source>
        <translation>已运行: --:--:--</translation>
    </message>
    <message>
        <source>ETA: --:--:--</source>
        <translation>预计剩余: --:--:--</translation>
    </message>
    <message>
        <source>Elapsed: %1</source>
        <translation>已运行: %1</translation>
    </message>
    <message>
        <source>ETA: %1</source>
        <translation>预计剩余: %1</translation>
    </message>
</context>

<!-- ═══ StorageWidget ═══ -->
<context>
    <name>StorageWidget</name>
    <message>
        <source>Octa-NAND Storage</source>
        <translation>NAND 闪存</translation>
    </message>
    <message>
        <source>Used: -- GB / -- GB
Free: -- GB</source>
        <translation>已用: -- GB / -- GB
可用: -- GB</translation>
    </message>
    <message>
        <source>Bad Blocks: 0 / 4096</source>
        <translation>坏块: 0 / 4096</translation>
    </message>
    <message>
        <source>Used: %1 / %2
Free: %3</source>
        <translation>已用: %1 / %2
可用: %3</translation>
    </message>
    <message>
        <source>Bad Blocks: %1 / %2</source>
        <translation>坏块: %1 / %2</translation>
    </message>
</context>

<!-- ═══ AlarmPopup ═══ -->
<context>
    <name>AlarmPopup</name>
    <message>
        <source>ACKNOWLEDGE</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Value: %1 %2  |  Threshold: %3 %2</source>
        <translation>数值: %1 %2  |  阈值: %3 %2</translation>
    </message>
</context>

</TS>
