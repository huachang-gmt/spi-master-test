
1. `gpio25_interrupt_test.cpp`
   → **只驗證 CM5 GPIO25 rising-edge interrupt / notification**

2. `spi_master_16byte_test.cpp`
   → **只驗證 CM5 SPI Master 能不能透過 `/dev/spidev0.0` 取得 STM32 SPI Slave 的 16-byte 資料**

3. `spi_master_irq_16byte_test.cpp`
   → **把前兩個驗證整合起來：GPIO25 notification → SPI transaction → sequence verification**
   → 這就是目前正式的 **16-byte IRQ-triggered SPI baseline**
   → 以下說明針對此檔案。
   → 編譯與執行方法在最後面。

---

# Raspberry Pi CM5 SPI Master Test

## STM32H755 SPI2 Slave + PE3 Notification + CM5 GPIO25 Interrupt

---

## 1. Project Overview

本專案用於驗證：

> **Raspberry Pi CM5 作為 SPI Master，接收 STM32H755 SPI2 Slave 傳送的資料。**

目前的硬體架構為：

```text
STM32H755 CM7
     │
     │ SPI2 Slave
     │
     ├──────── SPI SCK ───────────────► CM5 SPI Master
     ├──────── SPI MOSI ──────────────► CM5 SPI Master
     ├──────── SPI MISO ◄────────────── CM5 SPI Master
     ├──────── SPI CS  ◄─────────────── CM5 CE0
     │
     │ PE3
     │
     └──────────────► CM5 GPIO25
                       │
                       │ Rising Edge
                       ▼
                  GPIO Interrupt
                       │
                       ▼
                  SPI Transfer
                       │
                       ▼
                  RX Data
                       │
                       ▼
                 Data Verify
```

目前已經完成第一階段：

```text
16-byte
IRQ-triggered
SPI Mode 0
1 MHz
STM32 SPI2 Slave
CM5 SPI Master
Sequence verification
```

並且已經實際 PASS。

---

# 2. Project Purpose

這個專案不是最終的 EtherCAT application。

目前的目的，是在正式接入 EtherCAT real data 之前，先把：

```text
GPIO notification
        +
SPI Master
        +
SPI Slave
        +
Data integrity
        +
Sequence verification
```

逐項驗證完成。

最終目標是：

```text
STM32H755
    │
    │ 每 1 ms 產生一筆 EtherCAT data
    │
    │ 204 Bytes
    ▼
SPI2 Slave
    │
    ▼
CM5 GPIO25 notification
    │
    ▼
CM5 SPI Master
    │
    ▼
204-byte RX
    │
    ▼
Sequence verification
```

最終必須確認：

* packet 不遺失
* packet 不重複
* packet 不錯序
* data 不錯位
* data 不損壞
* 長時間運作穩定

---

# 3. Related STM32 Project

CM5 端程式對應的 STM32 專案：

```text
STM32_SlaveSPI2_RPi5_MasterSPI
```

STM32 使用：

```text
STM32H755ZIT6U
NUCLEO-H755ZI-Q
```

STM32 執行：

```text
SPI2 = Slave
```

CM5 執行：

```text
SPI = Master
```

---

# 4. Hardware Connection

## 4.1 SPI Connection

目前 STM32H755 SPI2 與 Raspberry Pi CM5 的連接如下。

| Signal               | STM32H755      | Raspberry Pi CM5 |
| -------------------- | -------------- | ---------------- |
| SPI Clock            | SPI2_SCK       | GPIO11 / SCLK    |
| SPI Data STM32 → CM5 | SPI2_MISO      | GPIO9 / MISO     |
| SPI Data CM5 → STM32 | SPI2_MOSI      | GPIO10 / MOSI    |
| SPI Chip Select      | PD14 / SPI2_CS | GPIO8 / CE0      |
| Notification         | PE3            | GPIO25           |
| Ground               | GND            | GND              |

也就是：

```text
STM32H755                         Raspberry Pi CM5
──────────                         ────────────────

SPI2_SCK      ──────────────────► GPIO11 / SCLK

SPI2_MISO     ──────────────────► GPIO9 / MISO

SPI2_MOSI     ◄────────────────── GPIO10 / MOSI

PD14 / CS     ◄────────────────── GPIO8 / CE0

PE3           ──────────────────► GPIO25

GND           ─────────────────── GND
```

### 非常重要

STM32 是：

```text
SPI Slave
```

CM5 是：

```text
SPI Master
```

所以：

> **SPI clock 是由 CM5 Master 產生，不是 STM32 產生。**

CM5 收到 PE3 notification 後，才啟動 SPI transaction。

---

# 5. PE3 Notification

STM32 使用：

```text
PE3
```

通知 CM5：

> STM32 已經準備好一筆 SPI data。

CM5 使用：

```text
GPIO25
```

接收這個 notification。

目前 CM5 設定：

```text
GPIO chip : /dev/gpiochip0
GPIO line : 25
Edge      : RISING
```

因此：

```text
STM32 PE3

LOW
 │
 │
 └────── HIGH
          │
          ▼
     CM5 GPIO25
          │
          ▼
    RISING EDGE
          │
          ▼
   C++ program wakes
          │
          ▼
     SPI transfer
```

---

# 6. Why GPIO25 Is Used

SPI 本身只負責：

```text
SCK
MOSI
MISO
CS
```

SPI 並不知道：

> 「STM32 現在是不是已經準備好下一個 packet？」

因此額外使用 PE3/GPIO25 作為：

```text
DATA READY / NOTIFICATION
```

目前的定義是：

```text
PE3 rising edge
=
STM32 有一筆 data 可以讓 CM5 讀取
```

---

# 7. SPI Configuration

目前測試設定：

```text
SPI device : /dev/spidev0.0
SPI mode   : 0
SPI speed  : 1,000,000 Hz
bits       : 8
```

也就是：

```text
Mode 0
CPOL = 0
CPHA = 0
```

CM5 C++ 程式使用：

```cpp
SPI_IOC_MESSAGE(1)
```

執行 SPI transaction。

---

# 8. Directory Structure

目前資料夾：

```text
spi-master-test/
```

包含：

```text
spi-master-test/
│
├── gpio25_interrupt_test.cpp
├── spi_master_16byte_test.cpp
├── spi_master_irq_16byte_test.cpp
│
└── README.md
```

三個程式並不是三份重複程式。

它們代表 SPI bring-up 的三個不同階段。

---

# 9. gpio25_interrupt_test.cpp

## Purpose

這是最早期的 GPIO notification 測試程式。

主要目的：

> **單獨確認 STM32 PE3 → CM5 GPIO25 的硬體 notification 是否正常。**

此程式不負責完整 SPI data transfer。

它主要驗證：

```text
STM32 PE3
    │
    ▼
CM5 GPIO25
    │
    ▼
Rising Edge Detection
    │
    ▼
C++ program
```

---

## GPIO Configuration

使用：

```text
/dev/gpiochip0
```

GPIO：

```text
GPIO25
```

edge：

```text
RISING
```

因此 STM32：

```text
PE3 LOW → HIGH
```

CM5 應該收到：

```text
GPIO25 RISING
```

---

## Why This Test Exists

在 SPI 測試之前，必須先確定 notification 本身是正常的。

否則如果後面的 SPI 測試失敗，就無法判斷問題來自：

```text
GPIO
```

還是：

```text
SPI
```

因此先把 GPIO notification 單獨驗證。

---

# 10. spi_master_16byte_test.cpp

## Purpose

第二個測試程式用來驗證：

> **CM5 SPI Master 能否透過 `/dev/spidev0.0` 與 STM32 SPI2 Slave 完成 16-byte transaction。**

這個測試的重點是 SPI 本身。

基本流程：

```text
CM5
 │
 │ SPI Master transaction
 ▼
STM32 SPI2 Slave
 │
 │ 16 bytes
 ▼
CM5 RX buffer
 │
 ▼
Data verification
```

---

## Difference From GPIO Test

這個程式主要驗證：

```text
SPI Master
   ↓
SPI Slave
   ↓
16-byte RX
```

而不是：

```text
GPIO interrupt
```

因此它可以把：

```text
GPIO notification
```

與：

```text
SPI transfer
```

兩個問題分開。

---

# 11. spi_master_irq_16byte_test.cpp

這是目前最重要的測試程式。

它將前面兩個測試整合成完整流程：

```text
PE3 notification
       ↓
GPIO25 RISING
       ↓
C++ program wake-up
       ↓
SPI Master transaction
       ↓
STM32 SPI2 Slave
       ↓
16-byte RX
       ↓
Sequence verification
       ↓
Statistics
```

這也是目前 GitHub 應該標記為：

> **Current validated 16-byte baseline**

的程式。

---

# 12. spi_master_irq_16byte_test.cpp Configuration

目前：

```cpp
static constexpr const char* GPIO_CHIP = "/dev/gpiochip0";

static constexpr unsigned int GPIO_LINE = 25;

static constexpr const char* SPI_DEVICE =
    "/dev/spidev0.0";

static constexpr uint8_t SPI_MODE =
    SPI_MODE_0;

static constexpr uint32_t SPI_SPEED_HZ =
    1000000U;

static constexpr uint8_t SPI_BITS_PER_WORD =
    8U;

static constexpr size_t TEST_DATA_SIZE =
    16U;
```

因此：

```text
GPIO       : 25
SPI device : /dev/spidev0.0
SPI mode   : 0
SPI clock  : 1 MHz
bits       : 8
RX         : 16 bytes
```

---

# 13. Main Test Flow

程式啟動後：

```text
1. Open /dev/gpiochip0
        ↓
2. Configure GPIO25
        ↓
3. Rising edge detection
        ↓
4. Open /dev/spidev0.0
        ↓
5. Configure SPI Mode 0
        ↓
6. Configure 1 MHz
        ↓
7. Wait for PE3
```

程式會停在：

```text
Waiting for STM32H755 PE3...
```

此時沒有 GPIO event 就不會執行 SPI transaction。

---

# 14. GPIO Event

當 STM32：

```text
PE3 LOW → HIGH
```

CM5 收到：

```text
GPIO25 RISING
```

例如：

```text
[10:19:21] GPIO25 RISING  IRQ=1
```

此時：

```text
irq_count++
```

然後才啟動 SPI。

---

# 15. SPI Transaction

GPIO interrupt 發生後：

```text
[SPI] Starting Master transaction...
```

CM5 呼叫：

```cpp
spi.Transfer(...)
```

建立：

```cpp
struct spi_ioc_transfer
```

並執行：

```cpp
ioctl(
    fd_,
    SPI_IOC_MESSAGE(1),
    &transfer);
```

完成 16-byte SPI transfer。

---

# 16. RX Data

STM32 目前傳送：

```text
Sequence Number
Reserved
01
02
03
...
0E
```

例如第一筆：

```text
00 00 01 02 03 04 05 06
07 08 09 0A 0B 0C 0D 0E
```

第二筆：

```text
01 00 01 02 03 04 05 06
07 08 09 0A 0B 0C 0D 0E
```

第三筆：

```text
02 00 01 02 03 04 05 06
07 08 09 0A 0B 0C 0D 0E
```

---

# 17. Sequence Number

目前 Byte 0 是：

```text
Sequence Number
```

目的不是 EtherCAT protocol。

它是為了驗證：

> **SPI data 是否遺失、重複或順序錯誤。**

例如正常：

```text
0
1
2
3
4
5
```

代表：

```text
No missing
No duplicate
No out-of-order
```

---

# 18. Sequence Verification

CM5 會保存：

```text
expected sequence
```

如果：

```text
expected = 5
received = 5
```

則：

```text
PASS
```

如果：

```text
expected = 5
received = 6
```

則：

```text
missing packet
```

如果：

```text
expected = 5
received = 4
```

則可能是：

```text
duplicate
```

或：

```text
out-of-order
```

---

# 19. Statistics

目前程式會顯示：

```text
IRQ
SPI
PASS
FAIL
SEQ_ERR
MISSING
DUP/OOO
```

例如：

```text
[STAT] IRQ=4 SPI=4 PASS=4 FAIL=0
       SEQ_ERR=0 MISSING=0 DUP/OOO=0
```

這些 counter 是後續 204-byte / 1-ms 測試非常重要的診斷資訊。

---

# 20. Meaning of Counters

## IRQ

CM5 收到多少次：

```text
GPIO25 RISING
```

例如：

```text
IRQ=1000
```

代表 CM5 收到 1000 次 notification event。

---

## SPI

實際成功執行多少次：

```text
SPI transfer
```

理想狀態：

```text
IRQ == SPI
```

---

## PASS

SPI data verification 成功次數。

理想：

```text
PASS == SPI
```

---

## FAIL

SPI transfer 或 data verification 發生錯誤的次數。

理想：

```text
FAIL = 0
```

---

## SEQ_ERR

sequence verification 發生錯誤的次數。

理想：

```text
SEQ_ERR = 0
```

---

## MISSING

sequence 顯示有 packet 遺失。

理想：

```text
MISSING = 0
```

---

## DUP/OOO

Duplicate / Out-of-order。

理想：

```text
DUP/OOO = 0
```

---

# 21. Current Validated Result

目前最新的測試結果：

```text
herman@RPiCM5:~/spi-master-test $ ./spi_master_irq_16byte_test
========================================
CM5 SPI Master + GPIO25 Interrupt Test
========================================
GPIO chip : /dev/gpiochip0
GPIO line : 25
Edge      : RISING
SPI device: /dev/spidev0.0
SPI mode  : 0
SPI speed : 1000000 Hz
RX size   : 16 bytes
========================================
[SPI] Device       : /dev/spidev0.0
[SPI] Mode         : 0
[SPI] Bits         : 8
[SPI] Speed        : 1000000 Hz
[TEST] Expected RX : 00 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E
========================================
Waiting for STM32H755 PE3...
========================================
```

第一筆：

```text
[10:19:21] GPIO25 RISING  IRQ=1
[SPI] Starting Master transaction...
[SPI] RX data : 00 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E
[SEQ] Received=0
[VERIFY] PASS
[STAT] IRQ=1 SPI=1 PASS=1 FAIL=0 SEQ_ERR=0 MISSING=0 DUP/OOO=0
```

第二筆：

```text
[10:19:22] GPIO25 RISING  IRQ=2
[SPI] Starting Master transaction...
[SPI] RX data : 01 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E
[SEQ] Received=1
[VERIFY] PASS
[STAT] IRQ=2 SPI=2 PASS=2 FAIL=0 SEQ_ERR=0 MISSING=0 DUP/OOO=0
```

第三筆：

```text
[10:19:22] GPIO25 RISING  IRQ=3
[SPI] Starting Master transaction...
[SPI] RX data : 02 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E
[SEQ] Received=2
[VERIFY] PASS
[STAT] IRQ=3 SPI=3 PASS=3 FAIL=0 SEQ_ERR=0 MISSING=0 DUP/OOO=0
```

第四筆：

```text
[10:19:23] GPIO25 RISING  IRQ=4
[SPI] Starting Master transaction...
[SPI] RX data : 03 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E
[SEQ] Received=3
[VERIFY] PASS
[STAT] IRQ=4 SPI=4 PASS=4 FAIL=0 SEQ_ERR=0 MISSING=0 DUP/OOO=0
```

這代表：

```text
IRQ = 4
SPI = 4
PASS = 4
FAIL = 0
SEQ_ERR = 0
MISSING = 0
DUP/OOO = 0
```

---

# 22. Important Startup Synchronization Finding

早期測試曾經發生：

```text
SEQ=0
```

被 CM5 收到兩次。

現象：

```text
IRQ=1
SEQ=0
PASS

IRQ=2
SEQ=0
DUPLICATE / OUT-OF-ORDER
```

之後才：

```text
SEQ=1
SEQ=2
SEQ=3
...
```

這表示 STM32 與 CM5 在啟動時存在 synchronization timing 問題。

後續在 STM32 啟動測試前加入 startup delay 後，問題消失。

目前結果：

```text
SEQ=0
SEQ=1
SEQ=2
SEQ=3
...
```

連續正常。

因此 startup synchronization 是後續高頻率測試必須持續注意的項目。

---

# 23. Important: Current Test Is Not Yet 204-byte / 1-ms Validation

目前測試：

```text
Data size : 16 bytes
SPI       : 1 MHz
```

這是：

> **Bring-up baseline**

不是最終系統。

目前尚未證明：

```text
204 bytes
+
1 ms
+
1000 packets/sec
```

可以長時間零遺失運作。

---

# 24. 204-byte Timing Consideration

如果仍使用：

```text
SPI = 1 MHz
```

傳送 204 bytes：

```text
204 × 8
=
1632 bits
```

所需時間：

```text
1632 / 1,000,000
=
1.632 ms
```

因此：

```text
1 MHz
+
204 bytes
+
1 ms period
```

無法直接滿足最終 timing requirement。

所以進入 204-byte 測試後，需要逐步提高 SPI clock，並重新驗證：

```text
SPI transfer
GPIO notification
sequence
packet loss
```

---

# 25. Planned Test Phases

目前規劃：

```text
Phase 1
GPIO25 interrupt
        ↓
PASS
```

↓

```text
Phase 2
SPI Master 16-byte
        ↓
PASS
```

↓

```text
Phase 3
GPIO25 IRQ
        ↓
SPI Master
        ↓
16-byte
        ↓
Sequence verification
        ↓
PASS
```

目前已完成。

接下來：

```text
Phase 4
204-byte
low frequency
```

↓

```text
Phase 5
increase SPI clock
```

↓

```text
Phase 6
increase notification frequency
```

↓

```text
Phase 7
1 ms / 204 bytes
```

↓

```text
Phase 8
long-duration test
```

↓

```text
Phase 9
real EtherCAT data
```

---

# 26. Final Data Path

最終希望形成：

```text
                    STM32H755 CM7
                         │
                         │ EtherCAT data
                         │
                         ▼
                    204-byte packet
                         │
                         ▼
                     SPI2 Slave
                         │
                         │
                    PE3 HIGH
                         │
                         ▼
                  CM5 GPIO25 IRQ
                         │
                         ▼
                  SPI Master Start
                         │
                         ▼
                    204-byte RX
                         │
                         ▼
                   Sequence Check
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
            PASS       NO LOSS    IN ORDER
```

---

# 27. Data Integrity Requirements

正式測試時必須同時滿足：

```text
IRQ count
    =
SPI count
```

並且：

```text
SPI count
    =
PASS count
```

以及：

```text
SEQ_ERR = 0
MISSING = 0
DUP/OOO = 0
```

長時間測試也必須保持這些條件。

---

# 28. Build / Run

目前主要執行程式：

```text
spi_master_irq_16byte_test
```

執行：

```bash
./spi_master_irq_16byte_test
```

啟動後應看到：

```text
Waiting for STM32H755 PE3...
```

接著啟動或 reset STM32H755。

當 PE3：

```text
LOW → HIGH
```

CM5 應看到：

```text
GPIO25 RISING
```

然後：

```text
SPI transaction
```

最後：

```text
VERIFY PASS
```

---

# 29. Current Baseline

目前 GitHub 建議將：

```text
spi_master_irq_16byte_test.cpp
```

視為：

> **16-byte IRQ-triggered SPI validated baseline**

這個版本已經實際驗證：

```text
STM32 PE3
      ↓
CM5 GPIO25
      ↓
GPIO rising event
      ↓
SPI Master
      ↓
STM32 SPI2 Slave
      ↓
16-byte RX
      ↓
Sequence verification
      ↓
PASS
```

---

# 30. Development Rule

後續修改必須遵守：

> **不要破壞目前已知 PASS 的 16-byte baseline。**

每一次修改：

1. 只修改必要部分
2. 編譯
3. 執行
4. 確認結果
5. 比較 counter
6. 再進入下一階段

尤其進入：

```text
204 bytes
1 ms
```

之後，不能只看：

```text
PASS
```

還必須看：

```text
IRQ
SPI
PASS
FAIL
SEQ_ERR
MISSING
DUP/OOO
```

---

# 31. Current Status

| Test Item                   | Status      |
| --------------------------- | ----------- |
| GPIO25 input                | PASS        |
| GPIO25 rising edge          | PASS        |
| STM32 PE3 notification      | PASS        |
| SPI device `/dev/spidev0.0` | PASS        |
| SPI Mode 0                  | PASS        |
| SPI 1 MHz                   | PASS        |
| 16-byte SPI transfer        | PASS        |
| STM32 SPI2 Slave            | PASS        |
| CM5 SPI Master              | PASS        |
| RX data verification        | PASS        |
| Sequence verification       | PASS        |
| Missing packet detection    | Implemented |
| Duplicate/OoO detection     | Implemented |
| 204-byte transfer           | Pending     |
| High-frequency transfer     | Pending     |
| 1-ms / 204-byte             | Pending     |
| Long-duration test          | Pending     |
| Real EtherCAT data          | Pending     |

---

# 32. Conclusion

`spi-master-test` 是 STM32H755 SPI2 Slave bring-up 的 **Raspberry Pi CM5 Master 端測試專案**。

三個測試程式各自負責不同階段：

```text
gpio25_interrupt_test.cpp
        │
        ▼
GPIO notification validation


spi_master_16byte_test.cpp
        │
        ▼
SPI Master / Slave validation


spi_master_irq_16byte_test.cpp
        │
        ▼
GPIO notification
      +
SPI transfer
      +
Sequence verification
      +
Data integrity validation
```

目前第三個程式已經完成第一個完整 baseline：

```text
16-byte
SPI Mode 0
1 MHz
PE3 → GPIO25
IRQ-triggered SPI
Sequence verification
```

最新驗證結果：

```text
IRQ=4
SPI=4
PASS=4
FAIL=0
SEQ_ERR=0
MISSING=0
DUP/OOO=0
```

因此目前已確認：

> **CM5 可以等待 STM32H755 PE3 notification，在收到 GPIO25 rising edge 後，以 SPI Master 模式啟動 transaction，從 STM32H755 SPI2 Slave 正確取得 16-byte 資料，並透過 sequence number 驗證資料連續性。**

這個版本應作為後續：

```text
16 Byte
   ↓
204 Byte
   ↓
higher SPI clock
   ↓
high frequency
   ↓
1 ms
   ↓
EtherCAT real data
```

的基準版本。

# 編譯與執行
## 編譯

```bash
cd ~/spi-master-test

g++ -std=c++17 -Wall -Wextra -O2 \
    gpio25_interrupt_test.cpp \
    -lgpiodcxx -lgpiod \
    -o gpio25_interrupt_test
```

```bash
cd ~/spi-master-test

g++ -std=c++17 -Wall -Wextra -O2 \
    spi_master_16byte_test.cpp \
    -o spi_master_16byte_test
```

**主要程式**
```bash
cd ~/spi-master-test

g++ -std=c++17 -Wall -Wextra -O2 \
    spi_master_irq_16byte_test.cpp \
    -lgpiodcxx -lgpiod \
    -o spi_master_irq_16byte_test 
```

## 執行

```bash
cd ~/spi-master-test

./gpio25_interrupt_test  

./spi_master_16byte_test

./spi_master_irq_16byte_test   <<< 主要程式
```

# 硬體連接圖

![SPI_Connection_1](images/connection1.png)
![SPI_Connection_2](images/connection2.png)


# 更新版本
## [2026-08-25] 修改檔案 spi_master_irq_16byte_test.cpp
### 修改原因： 因應 每 1ms 資料快速送到，必須移除不必要的列印資訊，只保留每一秒鐘統計資訊
> 例如：

每秒統計時會顯示：
```bash
[1s] RX=1000 OK=999 DATA_ERR=1 SEQ_ERR=0 LOST=0 DUP=0
```

意思是：

- RX=1000：這 1 秒收到 1000 筆
- OK=999：其中 999 筆「資料內容 + sequence」都正確
- DATA_ERR=1：有 1 筆資料內容錯誤
- SEQ_ERR=0：sequence 沒有中斷
- LOST=0：沒有遺失封包
- DUP=0：沒有 duplicate / out-of-order

如果同一秒有 3 筆資料內容錯誤：

```bash
[1s] RX=1000 OK=997 DATA_ERR=3 SEQ_ERR=0 LOST=0 DUP=0
```

如果同一筆 packet 同時發生 data error + sequence error，則：

```bash
[1s] RX=1000 OK=998 DATA_ERR=1 SEQ_ERR=1 LOST=1 DUP=0
```

**最後會得到這種結果**

正常

```bash
[1s] RX=1000 OK=1000 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
```

SPI 傳輸失敗 2 次

```bash
[1s] RX=998 OK=998 SPI_ERR=2 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
```
這代表：
> STM32 通知了 1000 次，但只有 998 次 SPI transaction 成功。

資料內容錯 1 次

```bash
[1s] RX=1000 OK=999 SPI_ERR=0 DATA_ERR=1 SEQ_ERR=0 LOST=0 DUP=0
```
代表：

> SPI 都有成功，但有 1 個 packet 的資料內容錯誤。

Sequence 遺失 2 個 packet

```bash
[1s] RX=998 OK=996 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=2 LOST=2 DUP=0
```
代表：

> SPI 收到 998 筆，但 sequence 發現有 2 筆遺失。

## 輸出結果
```text
編譯 ：

herman@RPiCM5:~/spi-master-test $ g++ -std=c++17 -Wall -Wextra -O2 \
    spi_master_irq_16byte_test.cpp \
    -lgpiodcxx -lgpiod \
    -o spi_master_irq_16byte_test

執行 ： 
herman@RPiCM5:~/spi-master-test $ ./spi_master_irq_16byte_test

輸出 ：

========================================
CM5 SPI Master + GPIO25 Interrupt Test
========================================
GPIO chip : /dev/gpiochip0
GPIO line : 25
Edge      : RISING
SPI device: /dev/spidev0.0
SPI mode  : 0
SPI speed : 1000000 Hz
RX size   : 16 bytes
========================================
[SPI] Device       : /dev/spidev0.0
[SPI] Mode         : 0
[SPI] Bits         : 8
[SPI] Speed        : 1000000 Hz
[TEST] Expected RX : 00 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E
========================================
Waiting for STM32H755 PE3...
========================================
[1s] RX=1 OK=1 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 OK=1003 SPI_ERR=0 DATA_ERR=0 SEQ_ERR=0 LOST=0 DUP=0


```
持續很多秒都完全一致。

這代表：


| 項目       |     結果 | 意義                               |
| -------- | -----: | -------------------------------- |
| RX       | 1003/s | CM5 每秒完成約 1003 次 SPI transaction |
| OK       | 1003/s | 每一筆 sequence + data 都正確          |
| SPI_ERR  |      0 | 沒有 SPI transaction failure       |
| DATA_ERR |      0 | 16 Byte 資料內容完全正確                 |
| SEQ_ERR  |      0 | sequence 完全連續                    |
| LOST     |      0 | 沒有遺失 packet                      |
| DUP      |      0 | 沒有 duplicate / out-of-order      |

**這是非常乾淨的結果。**

---
## [2026-08-25] 修改檔案 spi_master_irq_16byte_test.cpp
### 修改原因： 接收來自 STM32H755 以 每 1ms 傳送 204 byte 資料封包
- 移除不必要的列印資訊。
- 移除 VerifyData 驗證資料是否正確等不必要的 function
- 測試結果 ：
```text
編譯 ： 
herman@RPiCM5:~/spi-master-test $ g++ -std=c++17 -Wall -Wextra -O2 \
    spi_master_irq_16byte_test.cpp \
    -lgpiodcxx -lgpiod \
    -o spi_master_irq_16byte_test

執行 ： 
herman@RPiCM5:~/spi-master-test $ ./spi_master_irq_16byte_test

輸出結果 ： 
========================================
CM5 SPI Master + GPIO25 Interrupt Test
========================================
GPIO chip : /dev/gpiochip0
GPIO line : 25
Edge      : RISING
SPI device: /dev/spidev0.0
SPI mode  : 0
SPI speed : 1000000 Hz
RX size   : 204 bytes
========================================
[SPI] Device       : /dev/spidev0.0
[SPI] Mode         : 0
[SPI] Bits         : 8
[SPI] Speed        : 1000000 Hz
========================================
Waiting for STM32H755 PE3...
========================================
[1s] RX=1 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=576 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0

```
###  檢討
- 目前沒有看到 SPI 封包遺失，也沒有 sequence 中斷。但是有一個非常重要的現象：時間上還來得及「接收」，但現在的 SPI clock 速度明顯不足以支撐 1 ms / 204 bytes。
- 目前結果 ： CM5 每秒穩定得到：
```text
RX=576
SPI_ERR=0
SEQ_ERR=0
LOST=0
DUP=0
```
代表：

* RX=576：每秒完成約 576 個 204-byte SPI transaction
* SPI_ERR=0：沒有 SPI transfer error
* SEQ_ERR=0：sequence 完全連續
* LOST=0：沒有偵測到遺失 packet
* DUP=0：沒有 duplicate / out-of-order

> 所以就目前已經跑出的這一段而言： 資料沒有遺失，sequence 沒有中斷。

---

## 1. STM32 的「每 1 ms 一包」

這是**資料產生週期**：

```text
每 1 ms
   ↓
產生一個 204-byte packet
   ↓
sequence +1
```

這是你 STM32H755 的 Timer / `HAL_GetTick()` 所決定的。

---

## 2. SPI 的 `1,000,000 Hz`

這是**資料傳輸速度**，不是「每 1 ms 傳一次」。

你現在：

```cpp
static constexpr uint32_t SPI_SPEED_HZ = 1000000U;
```

意思是：

> SPI SCK 每秒振盪 1,000,000 次。

SPI 每一個 clock 傳 1 bit，所以：

```text
1 MHz = 1,000,000 bits/sec
```

而你現在一包是：

```text
204 bytes × 8 = 1,632 bits
```

因此：

```text
1,632 bits ÷ 1,000,000 bits/sec
≈ 1.632 ms
```

所以 **204 bytes 在 1 MHz 下，光 SPI clock 就需要約 1.632 ms 才傳得完。**

---

# 所以兩個 1 ms 是這樣的關係

假設 STM32：

```text
0 ms     packet #0
1 ms     packet #1
2 ms     packet #2
3 ms     packet #3
```

但是 CM5 SPI 只有 1 MHz：

```text
0 ms
 └────── 1.632 ms ──────┘
       packet #0

1.632 ms
 └────── 1.632 ms ──────┘
       packet #1

3.264 ms
...
```

**CM5 的 SPI 傳輸速度追不上 STM32 每 1 ms 產生資料的速度。**

---

# 可以設定更高嗎？

**可以，而且應該提高。**

例如：

### 2 MHz

```text
204 × 8 ÷ 2,000,000
= 0.816 ms
```

已經小於 1 ms。

### 4 MHz

```text
204 × 8 ÷ 4,000,000
= 0.408 ms
```

### 8 MHz

```text
204 × 8 ÷ 8,000,000
= 0.204 ms
```

所以：

| SPI Speed | 204 bytes 理論傳輸時間 |
| --------: | ---------------: |
|     1 MHz |       1.632 ms ❌ |
|     2 MHz |      0.816 ms ⚠️ |
|     4 MHz |       0.408 ms ✅ |
|     8 MHz |       0.204 ms ✅ |

注意這是**純 SPI clock 傳輸時間**，實際還有 Linux、GPIO event、`ioctl()` 等 overhead。

---

## 我建議我們現在怎麼做？

你現在的：

```cpp
static constexpr uint32_t SPI_SPEED_HZ = 1000000U;
```

**先不要丟掉，因為它是很好的 baseline。**

我們已經得到：

```text
1 MHz
RX=576
SPI_ERR=0
SEQ_ERR=0
LOST=0
DUP=0
```

接下來我建議直接測：

```cpp
static constexpr uint32_t SPI_SPEED_HZ = 4000000U;
```

也就是 **4 MHz**。

然後重新跑同一個測試。

我們最想看到的是：

```text
[1s] RX=??? SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
```

如果接近：

```text
RX ≈ 1000
```

而且：

```text
SPI_ERR=0
SEQ_ERR=0
LOST=0
DUP=0
```

那就代表 **SPI bandwidth 已經足夠，接下來才真正值得追查 STM32 的 1 ms timing 與 blocking 問題。**

所以答案很簡單：

> **SPI speed 跟 1 ms 有關，但它不是「1 ms 定時器」。它決定的是 204 bytes 能不能在這 1 ms 的時間窗口內傳完。**

而且**4 MHz 是我們現在非常合理的下一個測試點。**


---
原設定數值
static constexpr uint32_t SPI_SPEED_HZ = 1000000U;
修改設定數值
static constexpr uint32_t SPI_SPEED_HZ = 4000000U;

更改成 4MHz 之後
輸出結果：

```text
herman@RPiCM5:~/spi-master-test $ ./spi_master_irq_16byte_test
========================================
CM5 SPI Master + GPIO25 Interrupt Test
========================================
GPIO chip : /dev/gpiochip0
GPIO line : 25
Edge      : RISING
SPI device: /dev/spidev0.0
SPI mode  : 0
SPI speed : 4000000 Hz
RX size   : 204 bytes
========================================
[SPI] Device       : /dev/spidev0.0
[SPI] Mode         : 0
[SPI] Bits         : 8
[SPI] Speed        : 4000000 Hz
========================================
Waiting for STM32H755 PE3...
========================================
[1s] RX=3 SPI_ERR=0 SEQ_ERR=3 LOST=0 DUP=3
[1s] RX=2 SPI_ERR=0 SEQ_ERR=2 LOST=0 DUP=2
[1s] RX=2 SPI_ERR=0 SEQ_ERR=2 LOST=0 DUP=2
[1s] RX=2 SPI_ERR=0 SEQ_ERR=2 LOST=0 DUP=2
[1s] RX=2 SPI_ERR=0 SEQ_ERR=2 LOST=0 DUP=2
[1s] RX=2 SPI_ERR=0 SEQ_ERR=2 LOST=0 DUP=2
[1s] RX=2 SPI_ERR=0 SEQ_ERR=2 LOST=0 DUP=2

```
**這個情況很不對**

**這個結果非常不對，而且它告訴我們一個很重要的事情：現在已經不是單純的「1 MHz 太慢」問題。**

先不要改 STM32，也不要急著改 CM5 程式。先解讀這個結果。

### 目前最重要的現象

4 MHz、204 bytes 理論 SPI wire time：

**約 0.408 ms**

所以從純粹 SPI 頻寬來看，1 ms 週期是足夠的。

但是實際卻變成：

```text
[1s] RX=3  SPI_ERR=0  SEQ_ERR=3  LOST=0  DUP=3
[1s] RX=2  SPI_ERR=0  SEQ_ERR=2  LOST=0  DUP=2
[1s] RX=2  SPI_ERR=0  SEQ_ERR=2  LOST=0  DUP=2
...
```

也就是 **CM5 每秒只真正完成 2～3 次 SPI transaction**。

這與：

```text
1 MHz → RX=576
4 MHz → RX=2
```

完全不是正常的比例關係。

---

## 我現在最懷疑的是「STM32 PE3 的觸發方式」

注意我們的架構：

```text
STM32
  │
  │ 每 1 ms
  ▼
PE3 HIGH
  │
  ▼
CM5 GPIO25 Rising Event
  │
  ▼
userspace 被喚醒
  │
  ▼
SPI 204 bytes
```

問題是：

**PE3 如果只是 HIGH 一下，然後很快又 LOW，再每 1 ms HIGH 一次，CM5 userspace 不一定能按照「一個 HIGH = 一個 SPI transaction」完整處理。**

而現在 4 MHz SPI transaction 本身只需要約 0.408 ms。

所以理論上應該可以。

但是我們現在看到的是：

```text
RX ≈ 2/s
```

這已經不是 SPI 傳輸時間能解釋的。

---

## 還有一個非常關鍵的線索：`DUP`

你現在看到：

```text
RX=2
SEQ_ERR=2
DUP=2
```

這代表：

**CM5 確實讀到了 SPI 資料，但是讀到的 sequence 並不是我們預期的下一個 sequence。**

而且 `LOST=0`。

這個組合非常值得注意。

如果 STM32 正常：

```text
100
101
102
103
104
...
```

CM5 應該看到：

```text
100 → 101 → 102 → 103
```

如果 CM5 漏掉：

```text
100
102
103
```

就會：

```text
SEQ_ERR > 0
LOST = 1
```

但現在：

```text
SEQ_ERR > 0
LOST = 0
DUP > 0
```

比較像是：

```text
100
100
100
```

或者：

```text
100
99
100
```

也就是 **STM32 端提供給 SPI Slave 的資料/sequence 更新，與 PE3 notification 的時序沒有對上。**

---

# 但現在先不要下結論

這次測試反而非常有價值。

我們現在已經知道：

### 1 MHz

```text
204 bytes
SPI wire time ≈ 1.632 ms

RX ≈ 576/s
```

這個結果合理。

### 4 MHz

```text
204 bytes
SPI wire time ≈ 0.408 ms

RX ≈ 2/s
```

**這個結果完全不合理。**

所以現在不能再用：

> 「STM32 每 1 ms 不準」

來解釋全部問題。

因為如果只是 STM32 timer 不準，應該仍然看到幾百甚至接近 1000 次，而不是突然掉到 2 次。

---

# 我建議現在立刻做一個非常小的確認

**不要修改程式。**

先把 STM32H755 的程式貼給我目前正在執行的完整：

```c
SPI2_Slave_SendPacket()
```

以及：

```c
while (1)
{
    ...
}
```

中目前 **204-byte packet + PE3** 的完整程式。

尤其我要確認這幾件事情：

```text
1. 204-byte buffer 在哪裡更新
2. sequence number 在哪裡更新
3. SPI2_Slave_SendPacket() 做什麼
4. PE3 什麼時候拉 HIGH
5. PE3 什麼時候拉 LOW
6. SPI2 CS 是誰控制
7. SPI2_Slave_SendPacket() 是否 blocking
8. 每 1 ms 到底是誰決定的
```

**先不要修改任何東西。**

你現在這個 `4 MHz → RX=2` 的結果，很可能讓我們抓到目前 STM32 **「資料更新 / PE3 / SPI transaction」三者時序上的真正問題**。

而且你剛才的目標是今天下班前完成 **204 bytes / 1 ms 壓力測試**；我認為這個方向還來得及，不需要現在收尾。

---
## [2026-08-26] 修改檔案 spi_master_irq_16byte_test.cpp
### 修改原因： 調整 CM5 SPI_SPEED_HZ = 2000000U; 
**變更 SPI Clock 為 2MHz**

輸出結果 ：

```text
編譯 ： 
herman@RPiCM5:~/spi-master-test $ g++ -std=c++17 -Wall -Wextra -O2 \
    spi_master_irq_16byte_test.cpp \
    -lgpiodcxx -lgpiod \
    -o spi_master_irq_16byte_test

執行 ：
herman@RPiCM5:~/spi-master-test $ ./spi_master_irq_16byte_test

輸出結果 ： 
========================================
CM5 SPI Master + GPIO25 Interrupt Test
========================================
GPIO chip : /dev/gpiochip0
GPIO line : 25
Edge      : RISING
SPI device: /dev/spidev0.0
SPI mode  : 0
SPI speed : 2000000 Hz
RX size   : 204 bytes
========================================
[SPI] Device       : /dev/spidev0.0
[SPI] Mode         : 0
[SPI] Bits         : 8
[SPI] Speed        : 2000000 Hz
========================================
Waiting for STM32H755 PE3...
========================================
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0
[1s] RX=1003 SPI_ERR=0 SEQ_ERR=0 LOST=0 DUP=0

```
> 從輸出結果可以看出 接收數 達到 理論 1000 / 1 sec，因為每 1ms 傳送 一次，1 秒 就應該傳送 1000 次。

計算時間：

```text
204 × 8 / 2,000,000
= 0.816 ms
```
所以：
```text
SPI transfer ≈ 0.816 ms
```
已經小於：
```text
STM32 packet period = 1 ms
```
因此：
```text
0.816 ms < 1 ms
```
理論上已經可以每 1 ms 完成一次 transaction。

實際：
```text
RX ≈ 1003/s
```
而且：
```text
SPI_ERR=0
SEQ_ERR=0
LOST=0
DUP=0
```
這是一個非常漂亮的結果。

它證明目前這套架構：
```text
STM32
  ↓
204 bytes
  ↓
PE3
  ↓
CM5 GPIO25
  ↓
spidev
  ↓
204-byte SPI transaction
```
在 2 MHz 已經可以穩定跑到約 1000 packet/s。

> 「目前這套 STM32H755 SPI2 Slave + PE3 notification + CM5 spidev 架構，已實驗證明 2 MHz 穩定可靠、2.2 MHz 也可靠，而 2.5 MHz 已出現 sequence integrity failure。因此目前應將 2 MHz 視為保守且可靠的工作頻率，不再以提高 SPI clock 作為目前的優化方向。」

---
# [2026-08-31] 修改 檔案： spi_master_irq_16byte_test.cpp

我的目標不是「每 4 秒固定產生一個固定大小的檔案」，而是先把**尚未完成軌跡的 204-byte packet 持續累積**，到了建檔條件後，在一個完整 packet 傳輸結束的邊界建立兩個檔案。

我目前理解的設計如下：

### 1. CM5 每 1 ms 收到一個 204-byte packet

每個 packet：

```text
204 bytes
```

而且：

* 每個 1 ms 的 point 內容可能不同
* point 數量不固定
* 因此一次軌跡的總資料量不固定
* **不能預先假設檔案大小**

### 2. 現階段用「4 秒」模擬 DONE

暫時：

```text
每 4 秒觸發一次「準備建檔」
```

但有一個非常重要的規則：

> **4 秒到了不能直接切檔，必須等目前正在進行的 SPI 204-byte packet 完整接收完成。**

所以檔案切割點應該永遠落在：

```text
204-byte packet
```

的完整邊界。

不能出現：

```text
前 100 bytes → 舊檔案
後 104 bytes → 新檔案
```

### 3. 每次軌跡建立兩個檔案

第一個：

```text
/srv/ftp/LOG/trajLog.bin
```

特性：

* 永遠只有一份
* 每次完成一個軌跡就覆蓋
* 永遠代表**最新完成的軌跡**

第二個：

```text
/srv/ftp/LOG/trajLog_%YYYY-%MM-%DD-%H-%M-%S.bin
```

例如：

```text
trajLog_2026-08-31-16-20-35.bin
```

特性：

* 保存歷史軌跡
* 不覆蓋舊資料
* timestamp 使用**建立該軌跡檔案的時間**

而且：

```text
/srv/ftp/LOG/
```

底下**不再建立任何子資料夾**。

---

### 4. Buffer 大小不應該設定成「固定最大檔案大小」

這是你問題中非常重要的一點。

我不建議：

```cpp
uint8_t buffer[某個固定大小];
```

因為你已經明確知道：

> point 數量不固定 → 軌跡大小不固定。

比較適合使用 **動態成長的 buffer / container**，例如：

```cpp
std::vector<uint8_t>
```

每收到一個完整的 204-byte packet：

```text
vector.push_back / insert
```

持續累積。

例如：

```text
Point 1       204 bytes
Point 2       204 bytes
Point 3       204 bytes
...
Point N       204 bytes
```

最後：

```text
trajectory_buffer.size() = N × 204
```

因此完全不需要事先知道 N。

### 5. 但我會特別注意記憶體使用

這裡不能只是簡單地說「用 vector 就好了」。

因為未來真正的軌跡可能很長，所以修改 CM5 程式時，我會讓設計符合：

```text
SPI receive
    ↓
完整 204-byte packet
    ↓
暫存目前軌跡
    ↓
DONE / 4 秒條件成立
    ↓
確認 packet 已完整接收
    ↓
建立 trajLog.bin
    ↓
建立 trajLog_YYYY-MM-DD-HH-MM-SS.bin
```

而不是讓檔案資料在 SPI 傳輸過程中被切斷。

---

### 6. 最重要的是：這只是現在的「4 秒測試版」

我會把程式架構留成未來很容易替換：

```text
現在：
4 秒 → 建檔

未來：
收到 "DONE" → 建檔
```

**但不新增另一個給同事呼叫的複雜介面。**

未來同事的：

```text
DONE
```

進入命令解析程式後，只需要觸發相同的「trajectory complete」流程即可。

---

這次我建議**不要改動已經驗證成功的 GPIO25 → SPI 204 Byte 接收流程**。只在 SPI transaction 成功完成之後，把收到的 204 Byte 放進「目前軌跡暫存 buffer」，然後判斷是否到了建檔條件。

### 我會採用這個架構

```text
STM32 PE3 HIGH
       ↓
CM5 GPIO25 RISING
       ↓
SPI Receive 204 Byte
       ↓
SPI transaction 完成
       ↓
加入 trajectory_buffer
       ↓
【建檔條件】
目前暫定：累積時間 >= 4 秒
       ↓
建檔
 ┌───────────────┬─────────────────────────────┐
 │ trajLog.bin   │ trajLog_YYYY-MM-DD-HH-MM-SS │
 │ 覆蓋          │ 永久保存                     │
 └───────────────┴─────────────────────────────┘
       ↓
清空 trajectory_buffer
       ↓
開始收下一條軌跡
```

而且有一個很重要的地方：

> **4 秒到了，不會中斷 SPI。**

因為我們會在 `spi.Transfer()` 完成、204 Byte 已經完整收到並放入 buffer 後，才判斷 4 秒條件。因此你要求的「如果 4 秒剛好在 packet 傳輸中，等待 packet 完成」自然成立。

---

## Buffer 怎麼設計？

這裡不應該寫死例如：

```cpp
uint8_t buffer[1MB];
```

因為你已經明確說了：

> 點數不固定，所以軌跡檔案大小不固定。

因此 CM5 使用：

```cpp
std::vector<uint8_t> trajectory_buffer;
```

每收到一個 204 Byte：

```text
trajectory_buffer
    ↓
append 204 bytes
```

它會自動擴充。

例如：

```text
1000 points → 204,000 bytes
2000 points → 408,000 bytes
5000 points → 1,020,000 bytes
```

不需要事先知道點數。

而且我會讓 buffer **只儲存完整的 204-byte packet**，不會產生半個 packet。

---

# 這次修改的位置

我會改 **4 個地方**：

### ① Include 區域

增加：

```cpp
#include <fstream>
#include <ctime>
#include <vector>
```

---

### ② Configuration 區域

加入：

```cpp
static constexpr const char* LOG_DIRECTORY = "/srv/ftp/LOG";
static constexpr auto TRAJECTORY_INTERVAL =
    std::chrono::seconds(4);
```

並且用非常醒目的區塊標示：

```cpp
/* ============================================================
 * >>> TRAJECTORY COMPLETE CONDITION <<<
 *
 * CURRENT TEST CONDITION:
 *     4 seconds
 *
 * FUTURE:
 *     Replace this condition with DONE command detection.
 * ============================================================ */
```

**未來同事的 `DONE` command 做好後，只需要替換這個條件判斷，不需要修改 SPI 接收、buffer、建檔邏輯。**

---

### ③ Main event loop 裡 SPI 收到資料後

原本：

```cpp
++spi_count;
```

之後直接做 sequence verification。

我會在這裡加入：

```text
rx_data
   ↓
trajectory_buffer.insert(...)
   ↓
判斷 4 秒
   ↓
如果完成 → 建立兩個檔案
```

---

### ④ 增加檔案建立函式

這個函式只負責：

* `trajLog.bin` 覆蓋寫入
* `trajLog_YYYY-MM-DD-HH-MM-SS.bin` 保存
* 寫入完整 trajectory buffer
* 寫檔成功後清除 buffer

它**不是給你同事呼叫的 API**，只是 CM5 程式內部 implementation。

---

## 最重要的一點

目前這個程式的：

```cpp
GPIO25 RISING
        ↓
SPI transfer 204 bytes
```

我**不會改**。

你之前已經花很多時間驗證：

```text
12 MHz
204 bytes
約 1000 packet/sec
SPI_ERR=0
SEQ_ERR=0
LOST=0
DUP=0
```

所以這一段現在是我們的**已驗證基礎**。

這次只是在它後面增加：

```text
「資料收到了」
       ↓
「把資料暫存」
       ↓
「軌跡完成了嗎？」
       ↓
「建檔」
```

這樣最安全。

---

### 修改重點

1. `std::vector<uint8_t>` 作為軌跡暫存 buffer，不限制固定檔案大小。
2. 每成功收到一個完整 204-byte packet，就 append 到 buffer。
3. **只有 SPI transfer 完成後才檢查 4 秒條件**，因此不會切斷 packet。
4. 4 秒條件成立後：

   * `/srv/ftp/LOG/trajLog.bin`：覆蓋
   * `/srv/ftp/LOG/trajLog_YYYY-MM-DD-HH-MM-SS.bin`：永久保存
5. 建檔完成後清空 buffer，開始下一個 4 秒收集週期。
6. 原本 sequence verification 完全保留。
7. 原本 1 秒統計輸出完全保留。

### 有一個地方要特別注意

目前：

```cpp
trajectory_buffer.insert(...)
```

是在 **SPI 完整成功後**才執行，因此每筆都是完整的 204 Byte。

例如 4 秒期間收到：

```text
1001 packets
```

那麼檔案大小就是：

```text
1001 × 204 = 204204 bytes
```

下一次可能是 1002 packets：

```text
1002 × 204 = 204408 bytes
```

**完全不需要預先知道點數。**

而：

```cpp
trajectory_buffer.reserve(TEST_DATA_SIZE * 4096U);
```

只是先預留約 800 KiB 的容量，**不是檔案大小限制**。如果超過，`std::vector` 會自動擴充。

---

還有一個設計上的細節：目前 4 秒是從程式進入軌跡收集階段開始計算，而**真正切檔一定發生在某一個完整 204-byte packet 收到之後**。所以它不是「4.000 秒硬切」，而是「4 秒到了之後，等下一個完整 packet 完成再切」，這正符合你要求的行為。

---
## 添加 q 按鍵 按下後跳離程式執行

增加 **`q` 的正常離開機制**：

1. 程式一開始提示：
   `Press 'q' to exit after the current/final complete packet is received and logged.`

2. 按下 `q` 後：

   * 不直接 `break`
   * 如果當下 SPI 正在傳輸，讓 `SPI Transfer()` 完整結束
   * 完整 204 bytes 放入 `trajectory_buffer`
   * 建立：

     * `/srv/ftp/LOG/trajLog.bin`
     * `/srv/ftp/LOG/trajLog_YYYY-MM-DD-HH-MM-SS.bin`
   * **兩個檔案都成功建立後才離開**
   * 如果 `q` 是在兩個 packet 之間按下，則已經收到的最後完整 packet 就是最後一包，直接建立檔案後離開
   * 如果 `q` 按下時目前還沒有收到任何 packet，則繼續等待下一個 PE3 rising，收到完整 204 bytes 後才建檔並離開。

3. `q` 的偵測不會改變原本：
   **PE3 rising → SPI 204 bytes → buffer**
   的核心流程。

4. 原本的 **4 秒條件完全保留**，未來改成 `DONE` 時，仍然只需要修改你已經指定的：
   `>>> TRAJECTORY COMPLETE CONDITION <<<`
   區域。

5. 我會把修改位置明確標示，例如：

   * `// [MOD-1] q exit support`
   * `// [MOD-2] q exit input handling`
   * `// [MOD-3] graceful exit after final packet`

另外有一個重要的實作細節：**不能單純讓程式卡在原本的 `gpiod_line_request_wait_edge_events(..., -1)`**，否則使用者按 `q` 時，如果 PE3 沒有下一個 rising edge，程式根本沒有機會知道 `q` 被按下。

因此這次會把「等待 PE3」改成同時等待：

* GPIO25 event
* 標準輸入 `stdin`

而 **SPI 傳輸本身完全不改**。

這樣才能真正做到你要求的「運行中按 `q` 正常離開」，又不破壞目前已驗證成功的 SPI 接收架構。


核心做法是：

* `PE3 rising` 仍然是 SPI transaction 的唯一資料觸發來源。
* `q` 只負責設定「要求離開」旗標。
* `q` 不會中斷正在進行的 SPI transaction。
* 如果 `q` 按下時沒有 packet，程式會繼續等待下一個完整 204-byte packet。
* 收到最後完整 packet 後，一定先建立兩個檔案，再離開。
* 終端機使用非 canonical 模式，所以**直接按 `q` 不需要 Enter**。
* `q` 的處理與未來 `DONE` 條件完全分開，未來換掉 4 秒條件時不需要改 `q` 離開機制。

### 修改位置

共 4 個主要修改：

1. **Include 區域**：加入 `<poll.h>`、`<termios.h>`
2. **Configuration 後面**：加入終端機輸入設定與 `q` 狀態
3. **初始化 GPIO/SPI 後**：設定 stdin 為即時讀取 `q`
4. **Main event loop**：將原本永久等待 GPIO 的方式改成同時等待 GPIO25 與鍵盤；`q` 發生後仍完成最後一包並建檔。

### 這次最重要的行為

現在程式啟動後，會同時等待兩件事情：

```text
                    ┌── GPIO25 RISING ──→ SPI 204 bytes
                    │
CM5 event loop ─────┤
                    │
                    └── keyboard 'q' ──→ exit_requested = true
```

假設正在正常接收：

```text
PE3 ↑
  ↓
SPI 204 bytes
  ↓
加入 trajectory_buffer
  ↓
檢查 4 秒 / q
  ↓
建立 trajLog.bin
  ↓
建立 trajLog_YYYY-MM-DD-HH-MM-SS.bin
```

如果使用者在中間按 `q`：

```text
q
↓
exit_requested = true
↓
不立即離開
↓
等待下一個 PE3 ↑
↓
SPI 完整 204 bytes
↓
加入 buffer
↓
建立兩個檔案
↓
兩個檔案完成
↓
程式退出
```

**因此 `q` 不會造成半包資料，也不會在 SPI transaction 中途退出。**

另外我特別保留了你原本的 **4 秒條件區塊**，未來同事的 `DONE` 命令做好之後，只需要替換：

```cpp
const bool trajectory_complete =
    ((trajectory_now -
      trajectory_start_time) >=
     TRAJECTORY_INTERVAL);
```

這個條件即可；`q` 的正常離開機制不需要跟著改。

有一點你測試時要特別注意：**按 `q` 後，如果 STM32 已經停止送 PE3，CM5 會繼續等待下一個 PE3 rising，這是刻意如此設計的，因為你的要求是「一定要收完最後完整一包並建立檔案後才離開」。**

---

今天這一階段已經完成，而且目前程式運行結果符合你的預期。下面這份提示詞你明天直接貼到新對話即可，我把**今天完成的架構、已驗證結果、不可破壞的部分，以及明天要進行的 DONE 命令解析設計**都整理進去了。

# 明日新對話提示詞：CM5 SPI 接收 → Trajectory Log → DONE 命令解析

你現在要延續我昨天完成的 STM32H755 SPI Slave → Raspberry Pi CM5 SPI Master 資料接收與 Trajectory Log 儲存工作。

請先完整閱讀以下背景，不要重新設計已經驗證成功的 SPI 架構，也不要把已完成的部分重新做一次。

---

# 一、整體系統架構

目前系統為：

STM32H755 CM7
→ SPI2 Slave
→ Raspberry Pi CM5 SPI Master
→ GPIO25 notification
→ C++ userspace 程式
→ 收集 204-byte packet
→ 建立 Trajectory Log 檔案
→ FTP Server 根目錄 `/srv/ftp/LOG`

STM32H755 每 1 ms 產生一個資料點。

每一個資料點固定：

* 204 bytes
* Byte 0~1：16-bit sequence number
* Byte 2~203：EtherCAT application test data

未來同事的 EtherCAT 程式會讓這 204 bytes 隨著每個軌跡點而改變。

---

# 二、STM32H755 SPI Slave 已經完成，現在暫時不要修改

STM32H755 SPI Slave 這一階段已經完成並驗證成功。

主要架構：

* SPI2 Slave
* PE3 作為通知 CM5 的 GPIO
* CM5 GPIO25 接收 PE3 rising edge
* CM5 收到 GPIO25 rising edge 後才啟動 SPI transaction
* SPI 一次接收 204 bytes
* STM32 使用 DMA / non-blocking 架構
* Ring Buffer 架構已建立
* `HM_SendPacket()` 是目前主要送資料 API

STM32 端目前階段已經暫時結束，不要重新設計。

---

# 三、硬體波形已經確認

之前曾經使用錯誤的 PF2/PF3 作為測試 GPIO，後來確認 STM32H755 開發板沒有這些腳位。

最後改使用：

* PE2：測試波形輸出
* PE3：通知 CM5

實際示波器量測結果：

* PE2 HIGH 約 82 us
* PE2 下降緣附近緊貼 PE3 上升緣
* PE3 HIGH 約 0.24 ms

這證明目前 STM32 → CM5 的 notification / SPI transaction timing 架構符合預期。

---

# 四、CM5 SPI Master 已經驗證成功

目前 C++ 程式：

`spi_master_irq_16byte_test.cpp`

名稱雖然歷史上仍然叫 16byte_test，但實際目前已經是：

* SPI receive size = 204 bytes
* SPI mode = 0
* SPI speed = 12 MHz
* `/dev/spidev0.0`
* GPIO chip = `/dev/gpiochip0`
* GPIO line = 25
* GPIO25 rising edge 觸發 SPI transaction

SPI transaction 不是 timer polling。

流程固定：

STM32 PE3 HIGH
→ CM5 GPIO25 rising edge
→ C++ event wakeup
→ SPI transaction
→ 完整接收 204 bytes
→ packet 儲存
→ sequence verification
→ trajectory condition 判斷

這個架構已經成功運作。

---

# 五、目前已完成 Trajectory Log 功能

目前 CM5 程式已經可以把收到的 204-byte packet 持續收集。

因為：

「一條 trajectory 的 point 數量不固定」

所以沒有使用固定大小 buffer。

目前使用：

```cpp
std::vector<uint8_t> trajectory_buffer;
```

並且：

```cpp
trajectory_buffer.reserve(
    TEST_DATA_SIZE * 4096U);
```

這只是 initial allocation，不是最大容量。

vector 必要時會自動成長。

每收到一個完整 204-byte packet：

```cpp
trajectory_buffer.insert(
    trajectory_buffer.end(),
    rx_data,
    rx_data + TEST_DATA_SIZE);
```

所以 buffer 中的資料格式就是：

Packet 1

* Packet 2
* Packet 3
* ...
* Packet N

總大小：

`N × 204 bytes`

因此可以支援不固定的 trajectory point 數量。

---

# 六、目前的 Trajectory 完成條件

目前還沒有真正的命令解析程式。

所以昨天先使用「時間」模擬 trajectory complete condition。

目前：

```cpp
static constexpr auto TRAJECTORY_INTERVAL =
    std::chrono::seconds(4);
```

意思是：

每 4 秒把目前累積的 trajectory 視為完成。

非常重要：

條件判斷是在：

**完整 204-byte SPI transaction 完成之後**

才執行。

也就是：

GPIO25 rising
→ SPI 接收 204 bytes
→ transaction 完成
→ packet 放入 trajectory_buffer
→ sequence verification
→ 檢查 trajectory complete condition
→ 如果條件成立
→ 建立檔案

因此如果 4 秒到達時，SPI packet 正在傳輸：

**不會中斷這個 packet。**

一定會等完整 204 bytes 接收完成後，才建立 trajectory file。

這個行為已經符合需求。

---

# 七、目前建立兩個檔案

FTP Server LOG 目錄：

```text
/srv/ftp/LOG
```

不要建立任何額外子目錄。

## 1. 最新 trajectory

固定：

```text
/srv/ftp/LOG/trajLog.bin
```

每次 trajectory 完成：

* 覆蓋舊檔
* 寫入最新 trajectory
* 因此永遠代表最新一筆 trajectory

使用：

```cpp
std::ios::binary |
std::ios::trunc
```

---

## 2. 歷史 trajectory

格式：

```text
trajLog_YYYY-MM-DD-HH-MM-SS.bin
```

例如：

```text
trajLog_2026-08-31-14-35-27.bin
```

這個檔案：

* 每次 trajectory 完成建立一個新檔案
* 不應該覆蓋之前的 trajectory
* 用建立檔案時的 local time 命名

---

# 八、今天已經實際驗證成功

目前程式實際運行符合期待：

1. CM5 可以持續接收 STM32H755 的 204-byte packet
2. GPIO25 rising edge 可以正常觸發 SPI transaction
3. SPI 12 MHz 已經正常運作
4. 204-byte packet 可以完整接收
5. trajectory buffer 可以持續累積 packet
6. 每 4 秒可以建立 trajectory file
7. `trajLog.bin` 會更新為最新 trajectory
8. timestamp archive file 可以保留歷史資料
9. packet 傳輸中的情況，不會在 packet 中途建立檔案
10. 必須先完成最後一個完整 packet，再進行 trajectory file creation
11. 不固定 point 數量的需求已透過 dynamic vector 解決
12. 程式正常運行結果符合目前測試需求

所以：

**SPI 接收架構與 Trajectory Logging 這一階段視為完成。**

---

# 九、目前又新增「q 離開程式」功能

昨天另外完成一個需求：

程式運行期間可以按：

```text
q
```

要求離開程式。

但是：

**不能因為按下 q 就立即離開。**

正確行為：

使用者按下 `q`
→ 設定 exit request
→ 不再接受新的 trajectory processing / 結束要求
→ 如果目前沒有正在進行的 SPI transaction，仍然要確保目前最後完整 packet 的處理完成
→ 如果 q 發生在 SPI packet 傳輸期間，必須等待 SPI transaction 完整結束
→ 將最後完整 packet 放入 trajectory buffer
→ 建立最後 trajectory file
→ 完整寫入 `trajLog.bin`
→ 完整寫入 timestamp archive file
→ 確認 file creation 完成
→ 才離開程式

也就是：

**q = request exit，不是立即 abort。**

這個功能目前也已經實際運行符合期待。

程式一開始的提示框已經加入 q 離開說明。

---

# 十、目前最重要的下一階段：真正的命令解析

不要再修改 SPI 傳輸架構。

下一階段要開始設計：

**Command Parser / Command Server**

目前使用 4 秒 timer：

```cpp
TRAJECTORY_INTERVAL
```

只是為了測試 trajectory file creation。

真正產品需求不是「每 4 秒建檔」。

真正需求是：

同事的 EtherCAT 程式控制滑台。

滑台每 1 ms 產生一個 trajectory point。

每個 point = 204 bytes。

當一整條 trajectory 完成時，同事的程式會送出：

```text
DONE
```

代表：

**trajectory 已經形成，可以建立 trajectory log file。**

因此未來流程應該變成：

STM32H755
→ 204-byte point
→ CM5 SPI
→ trajectory_buffer

同時：

同事的 command
→ Command Parser
→ 收到 `DONE`
→ 設定 trajectory complete condition

然後：

下一個完整 SPI packet 接收完成
→ 將 packet 放入 trajectory_buffer
→ 檢查 DONE condition
→ 建立：

```text
trajLog.bin
```

以及：

```text
trajLog_YYYY-MM-DD-HH-MM-SS.bin
```

---

# 十一、非常重要的設計原則

未來命令條件改變時：

**只替換「Trajectory Complete Condition」相關部分。**

不要修改：

* SPI GPIO25 event detection
* SPI transfer
* 204-byte packet handling
* sequence verification
* trajectory_buffer
* file writing
* filename generation
* q graceful shutdown
* statistics

理想架構：

```text
                    +----------------------+
STM32 PE3 --------> | GPIO25 event         |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    | SPI Receive 204 bytes|
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    | trajectory_buffer    |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    | Command / Condition  |
                    |      CHECK           |
                    +----------+-----------+
                               |
                         DONE received?
                               |
                              YES
                               |
                               v
                    +----------------------+
                    | Create trajectory    |
                    | files                |
                    +----------------------+
```

未來如果命令從：

```text
DONE
```

改成其他 protocol，例如：

```text
DONE\n
```

或：

```text
CMD:DONE
```

或 binary command，

應該只需要修改 Command Parser / Condition 部分。

---

# 十二、明天第一個工作

明天開始前，先不要直接大改程式。

先分析目前 `spi_master_irq_16byte_test.cpp` 的結構，找出：

1. 哪裡最適合加入 Command Parser
2. Command Parser 是否應該獨立成 function
3. 是否需要另外的 thread / non-blocking stdin / IPC
4. `DONE` 到達時如何安全地通知主 SPI event loop
5. 如何保證 DONE 不會破壞目前的 204-byte SPI transaction
6. 如何保證 q graceful shutdown 與 DONE 不互相衝突
7. 如何讓未來替換命令格式時，只修改 command parser

目前不要為了 command parser 重新設計 SPI。

---

# 十三、特別注意：DONE 與 SPI packet 的時間關係

這是明天設計時非常重要的問題。

假設：

SPI 正在傳送第 N 個 204-byte packet。

同時收到：

```text
DONE
```

不能中斷第 N 個 packet。

必須：

```text
完成第 N 個 SPI packet
        ↓
將第 N 個 packet 放入 buffer
        ↓
再確認 DONE condition
        ↓
建立 trajectory file
```

因此 trajectory file 永遠只能包含完整的 204-byte packet。

不能產生：

* 半個 packet
* 被截斷的 packet
* packet 傳輸中途建立檔案

---

# 十四、q 與 DONE 的優先順序

明天設計 command parser 時也要考慮：

### 情況 A

收到：

```text
DONE
```

然後沒有 q：

→ 完成 trajectory
→ 建立兩個檔案
→ 繼續等待下一條 trajectory

### 情況 B

使用者按：

```text
q
```

：

→ request exit
→ 完成最後正在進行的 SPI packet
→ 將 packet 加入 buffer
→ 建立最後 trajectory file
→ 離開

### 情況 C

DONE 與 q 幾乎同時發生：

必須設計 deterministic behavior。

原則上：

**完整 packet 優先完成，file creation 必須 atomic at application level，最後才離開。**

明天需要針對這個情況討論並決定實際實作方式。

---

# 十五、不要忘記目前程式的測試條件

目前測試：

```cpp
static constexpr uint32_t SPI_SPEED_HZ = 12000000U;
```

```cpp
static constexpr size_t TEST_DATA_SIZE = 204U;
```

```text
SPI Mode = 0
GPIO25 = rising edge
```

這些都是目前已經驗證過的設定。

**不要為了 Command Parser 而修改。**

---

# 十六、目前階段的目標

現在已經完成：

```text
STM32H755 SPI Slave
        ↓
204-byte packet
        ↓
CM5 GPIO interrupt
        ↓
CM5 SPI Master
        ↓
204-byte receive
        ↓
dynamic trajectory buffer
        ↓
4-second test condition
        ↓
trajLog.bin
+
trajLog_YYYY-MM-DD-HH-MM-SS.bin
        ↓
q graceful shutdown
```

明天要進入：

```text
                    ┌── SPI 204-byte data ──→ trajectory buffer
                    │
STM32H755 ──────────┤
                    │
                    └── Command ──→ Command Parser
                                      │
                                      ↓
                                    DONE
                                      │
                                      ↓
                              Trajectory Complete
                                      │
                                      ↓
                              Create Log Files
```

最終目標：

**把現在的「4 秒時間條件」乾淨地替換成「DONE 命令條件」，而其他已經驗證成功的 SPI、buffer、file、sequence verification、statistics、q graceful shutdown 全部保持不變。**

# [2026-10-12] 更新

```
          【第一階段：CM5 啟動】

CM5 開機
   │
   ▼
啟動 SPI Master 接收程式
   │
   ├── 初始化 SPI Master
   ├── 設定 GPIO25 上升沿偵測
   └── 進入等待狀態
           │
           │ 此時 STM32H755 尚未啟動
           ▼
      等待 GPIO25 上升沿
           │
           │
          【第二階段：STM32 啟動】

STM32H755 開機
   │
   ▼
初始化 SPI Slave 與 GPIO
   │
   ▼
開始每 1ms 準備／傳送一筆資料
   │
   ▼
GPIO 輸出上升沿
   │
   ▼
CM5 偵測 GPIO25 上升沿
   │
   ▼
CM5 SPI Master 開始傳輸
   │
   ▼
接收 204 Bytes
   │
   ▼
等待下一次 GPIO 上升沿
   │
   └────────────── 每 1ms 重複
```

```
PC
└── GMT Command Client（Qt）
        │
        │ TCP :9999
        ▼
CM5
└── GMT_Command_Gateway
        │
        ├── USB CDC
        │     │
        │     ▼
        │   STM32H755
        │     │
        │     └── 執行運動命令
        │
        └── UDS Client
              │
              │ /tmp/gmt_trajectory.sock
              │ SAVE_TRAJECTORY\n
              ▼
        spi-master-test
              │
              ├── 接收 STM32H755 的 SPI 軌跡資料
              ├── 寫入 trajLog.bin
              ├── 建立時間戳記封存檔
              └── 回覆 SAVE_OK\n 或 SAVE_ERROR\n
```

```
1. Qt Client 傳送運動命令
          │
          ▼
2. Gateway 將命令傳送給 STM32H755
          │
          ▼
3. STM32H755 執行運動
          │
          ▼
4. STM32H755 回覆真正的 DONE
          │
          ▼
5. Gateway 將 DONE 回覆給 Qt Client
          │
          ▼
6. Gateway 傳送 SAVE_TRAJECTORY\n
   給 spi-master-test
          │
          ▼
7. spi-master-test 執行存檔
          │
          ▼
8. spi-master-test 回覆 SAVE_OK\n
   或 SAVE_ERROR\n
```


# SPI Master Test — STM32H755 Trajectory Logging

## 1. Overview

This project runs on a Raspberry Pi Compute Module 5 (CM5) and receives trajectory data from an STM32H755 microcontroller through SPI.

The CM5 acts as the SPI Master, while the STM32H755 acts as the SPI Slave.

The STM32H755 generates a GPIO trigger signal to notify the CM5 when SPI data should be retrieved. The CM5 detects the rising edge of GPIO25 and performs an SPI transfer to receive one trajectory packet.

The project also provides a Unix Domain Socket (UDS) interface that allows another local application, such as `GMT_Command_Gateway`, to request trajectory data to be saved to disk.

## 2. System Architecture

```text
STM32H755
    |
    | GPIO rising edge
    | SPI Slave: 204-byte trajectory packet
    v
Raspberry Pi CM5
    |
    +-- GPIO25 rising-edge detection
    |
    +-- SPI Master
    |      |
    |      +-- Receive trajectory packets
    |      +-- Append received data to memory buffer
    |
    +-- UDS Server
           |
           | /tmp/gmt_trajectory.sock
           |
           +-- Receive SAVE_TRAJECTORY command
           +-- Save trajectory data
           +-- Return SAVE_OK or SAVE_ERROR
```

## 3. Hardware and Software

### Hardware

* Raspberry Pi Compute Module 5 (CM5)
* STM32H755 microcontroller board
* SPI connection between CM5 and STM32H755
* GPIO25 trigger connection

### Software

* Linux on Raspberry Pi CM5
* C++17
* Linux SPI userspace interface (`spidev`)
* Linux GPIO character-device interface through `libgpiod`
* Unix Domain Socket (AF_UNIX)

## 4. SPI Configuration

| Parameter     | Configuration               |
| ------------- | --------------------------- |
| SPI device    | `/dev/spidev0.0`            |
| SPI role      | CM5 Master, STM32H755 Slave |
| SPI mode      | Mode 0                      |
| Bits per word | 8                           |
| SPI clock     | 12 MHz                      |
| Packet size   | 204 bytes                   |
| GPIO trigger  | GPIO25                      |
| Trigger edge  | Rising edge                 |

GPIO25 is the trigger for SPI data retrieval.

The CM5 waits for a GPIO25 rising edge and then performs one SPI transfer to retrieve a 204-byte packet.

The STM32H755 must generate the trigger signal according to the established hardware and firmware protocol.

## 5. Startup Sequence

The intended startup sequence is:

1. Start the CM5 and run `spi_master_irq_16byte_test`.
2. Confirm that the application has initialized SPI and GPIO monitoring and is listening on the UDS socket.
3. Power on the STM32H755.
4. The STM32H755 begins generating GPIO trigger events and providing SPI data.
5. The CM5 receives trajectory packets and appends them to its in-memory trajectory buffer.

This startup sequence was tested on the actual hardware.

The CM5 was started before the STM32H755. After the STM32H755 was powered on without manually pressing RESET, the production application received trajectory packets successfully.

## 6. UDS Interface

### Socket Path

```text
/tmp/gmt_trajectory.sock
```

### Save Command

The client sends the following command through the Unix Domain Socket:

```text
SAVE_TRAJECTORY\n
```

The command is sent as ASCII bytes, including the terminating newline character.

### Responses

| Response       | Meaning                                                               |
| -------------- | --------------------------------------------------------------------- |
| `SAVE_OK\n`    | The save operation completed successfully                             |
| `SAVE_ERROR\n` | The save operation failed or no trajectory data was available to save |

The client should read and check the response instead of assuming that sending the command means the save operation succeeded.

The UDS server is intended for local inter-process communication on the CM5. It is not a network TCP service.

## 7. Trajectory Log Files

Log directory:

```text
/srv/ftp/LOG
```

Latest trajectory log:

```text
/srv/ftp/LOG/trajLog.bin
```

Timestamped archive naming pattern:

```text
/srv/ftp/LOG/trajLog_YYYY-MM-DD-HH-MM-SS.bin
```

After a successful save, the application writes the trajectory data to the latest log and creates a timestamped archive.

The in-memory trajectory buffer is cleared after a successful save so that subsequent trajectory data can be collected for another save operation.

An empty trajectory buffer must not cause existing log files to be overwritten.

## 8. Hardware Verification Results

The following tests were completed on the actual CM5 and STM32H755 hardware.

| Test                                                               | Result |
| ------------------------------------------------------------------ | ------ |
| Start CM5 before STM32H755                                         | PASS   |
| Power on STM32H755 without manually pressing RESET                 | PASS   |
| Detect GPIO25 rising edges                                         | PASS   |
| Receive 204-byte SPI packets                                       | PASS   |
| Receive rate approximately 1,000 packets per second                | PASS   |
| SPI error counter remained at zero during the reported test        | PASS   |
| Sequence error counter remained at zero during the reported test   | PASS   |
| Lost packet counter remained at zero during the reported test      | PASS   |
| Duplicate packet counter remained at zero during the reported test | PASS   |
| Connect to the UDS server from a separate local process            | PASS   |
| Send `SAVE_TRAJECTORY\n`                                           | PASS   |
| Receive `SAVE_OK\n`                                                | PASS   |
| Create `trajLog.bin`                                               | PASS   |
| Create timestamped archive                                         | PASS   |
| Verify latest log and archive contents using SHA-256               | PASS   |

### Verified Save Operation

The following files were verified after a successful save on 2026-10-12.

Latest log:

```text
trajLog.bin
Size: 48,565,872 bytes
SHA-256: bf7a6d222ad177067106d42b2893a8f11ec45ec3e972492554be8da0cf185a29
```

Timestamped archive:

```text
trajLog_2026-10-12-16-14-18.bin
Size: 48,565,872 bytes
SHA-256: bf7a6d222ad177067106d42b2893a8f11ec45ec3e972492554be8da0cf185a29
```

The file sizes and SHA-256 hashes matched, confirming that both files contained identical data for this test.

These values describe the verified test run; they are not expected to remain the same in future runs.

## 9. Independent SPI Diagnostic Test

An independent diagnostic program, `spi_stm32_rx_test`, was created to test GPIO-triggered SPI reception separately from the production application.

The diagnostic program:

* Monitors GPIO25 rising edges.
* Performs one 204-byte SPI transfer for each detected edge.
* Reports received byte counts and packet samples.
* Reports GPIO edge, successful transfer, and SPI error counters.
* Does not write trajectory log files.
* Does not use the UDS save interface.
* Does not modify the production application's source code.

The diagnostic test confirmed that the CM5 could receive SPI data when the CM5 was started first and the STM32H755 was powered on afterward.

**Important:** The diagnostic program and production application both request GPIO25. Do not run them simultaneously.

## 10. Important Operational Notes

* Start the CM5 application before powering on the STM32H755.
* GPIO25 rising edges trigger SPI transfers.
* Do not run the independent diagnostic program concurrently with the production application.
* Do not issue another save command until the previous save operation has completed.
* A successful save clears the in-memory trajectory buffer.
* Verify the returned UDS response before treating a save as successful.
* Preserve existing trajectory logs unless there is an explicit reason to replace or delete them.

## 11. Integration Status

The SPI reception and UDS save functionality have passed the described hardware tests.

The next integration stage is to connect `GMT_Command_Gateway` to the UDS server.

The intended responsibility split is:

* `GMT_Command_Gateway`: decides when to request trajectory data to be saved.
* `spi_master_irq_16byte_test`: receives SPI data, stores trajectory data, and performs the file-save operation.

Gateway-triggered saving, including handling `SAVE_OK` and `SAVE_ERROR`, must be verified separately.

## 12. Main Application

Production application source:

```text
spi_master_irq_16byte_test.cpp
```

Production executable:

```text
spi_master_irq_16byte_test
```

Independent diagnostic source:

```text
spi_stm32_rx_test.cpp
```

Independent diagnostic executable:

```text
spi_stm32_rx_test
```
