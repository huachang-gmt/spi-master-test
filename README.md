
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

./spi_master_irq_16byte_test
```

# 硬體連接圖

![SPI_Connection_1](images/connection1.png)
![SPI_Connection_2](images/connection2.png)
