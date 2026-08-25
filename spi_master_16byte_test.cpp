#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{

constexpr const char* SPI_DEVICE = "/dev/spidev0.0";

constexpr uint8_t SPI_MODE = SPI_MODE_0;
constexpr uint8_t SPI_BITS_PER_WORD = 8;

/*
 * Start conservatively.
 *
 * We are doing hardware bring-up first.
 * Speed is not important yet.
 */
constexpr uint32_t SPI_SPEED_HZ = 100000;

constexpr size_t TEST_LENGTH = 16;

constexpr uint8_t EXPECTED_DATA[TEST_LENGTH] =
{
    0xA5, 0x5A, 0x01, 0x02,
    0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E
};

void PrintData(const uint8_t* data, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        std::cout
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(data[i]);

        if (i + 1 < length)
        {
            std::cout << ' ';
        }
    }

    std::cout << std::dec << '\n';
}

bool VerifyData(const uint8_t* data, size_t length)
{
    if (length != TEST_LENGTH)
    {
        return false;
    }

    for (size_t i = 0; i < TEST_LENGTH; ++i)
    {
        if (data[i] != EXPECTED_DATA[i])
        {
            std::cout
                << "[ERROR] Byte "
                << i
                << ": expected 0x"
                << std::hex
                << std::uppercase
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(EXPECTED_DATA[i])
                << ", received 0x"
                << std::setw(2)
                << static_cast<unsigned>(data[i])
                << std::dec
                << '\n';

            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    std::cout << "========================================\n";
    std::cout << "CM5 SPI Master 16-Byte Test\n";
    std::cout << "========================================\n";
    std::cout << "Device : " << SPI_DEVICE << '\n';
    std::cout << "Mode   : 0\n";
    std::cout << "Bits   : 8\n";
    std::cout << "Speed  : " << SPI_SPEED_HZ << " Hz\n";
    std::cout << "Length : " << TEST_LENGTH << " bytes\n";
    std::cout << "========================================\n";

    const int fd = open(SPI_DEVICE, O_RDWR);

    if (fd < 0)
    {
        std::cerr
            << "[ERROR] Cannot open "
            << SPI_DEVICE
            << ": "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    uint8_t mode = SPI_MODE;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        std::cerr
            << "[ERROR] SPI_IOC_WR_MODE failed: "
            << std::strerror(errno)
            << '\n';

        close(fd);
        return 1;
    }

    uint8_t bits = SPI_BITS_PER_WORD;

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
    {
        std::cerr
            << "[ERROR] SPI_IOC_WR_BITS_PER_WORD failed: "
            << std::strerror(errno)
            << '\n';

        close(fd);
        return 1;
    }

    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
        std::cerr
            << "[ERROR] SPI_IOC_WR_MAX_SPEED_HZ failed: "
            << std::strerror(errno)
            << '\n';

        close(fd);
        return 1;
    }

    uint8_t tx[TEST_LENGTH] = {};

    uint8_t rx[TEST_LENGTH] = {};

    /*
     * STM32 Slave does not need meaningful MOSI data
     * for this first test.
     *
     * Zero bytes are used only to generate clocks.
     */
    std::memset(tx, 0x00, sizeof(tx));

    spi_ioc_transfer transfer{};

    transfer.tx_buf =
        reinterpret_cast<uint64_t>(tx);

    transfer.rx_buf =
        reinterpret_cast<uint64_t>(rx);

    transfer.len = TEST_LENGTH;

    transfer.speed_hz = SPI_SPEED_HZ;

    transfer.bits_per_word = SPI_BITS_PER_WORD;

    std::cout << "[SPI] Starting transfer...\n";

    const int result =
        ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);

    if (result < 0)
    {
        std::cerr
            << "[ERROR] SPI transfer failed: "
            << std::strerror(errno)
            << '\n';

        close(fd);
        return 1;
    }

    std::cout
        << "[SPI] Transfer returned "
        << result
        << " bytes\n";

    std::cout << "[SPI] RX data : ";

    PrintData(rx, TEST_LENGTH);

    const bool valid =
        VerifyData(rx, TEST_LENGTH);

    if (valid)
    {
        std::cout << "[PASS] 16-byte data is correct.\n";
    }
    else
    {
        std::cout << "[FAIL] 16-byte data mismatch.\n";
    }

    close(fd);

    return valid ? 0 : 1;
}