#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <unistd.h>

#include <gpiod.h>

static constexpr const char* SPI_DEVICE = "/dev/spidev0.0";
static constexpr const char* GPIO_CHIP = "/dev/gpiochip0";

static constexpr unsigned int GPIO_LINE = 25;
static constexpr uint32_t SPI_SPEED_HZ = 12000000;
static constexpr uint8_t SPI_MODE_VALUE = SPI_MODE_0;
static constexpr uint8_t SPI_BITS = 8;
static constexpr size_t SPI_DATA_SIZE = 204;

static volatile std::sig_atomic_t stop_requested = 0;

static void HandleSignal(int)
{
    stop_requested = 1;
}

int main()
{
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    int spi_fd = -1;
    gpiod_chip* chip = nullptr;
    gpiod_line_settings* settings = nullptr;
    gpiod_line_config* line_config = nullptr;
    gpiod_request_config* request_config = nullptr;
    gpiod_line_request* line_request = nullptr;
    gpiod_edge_event_buffer* event_buffer = nullptr;

    uint64_t edge_count = 0;
    uint64_t transfer_count = 0;
    uint64_t transfer_error_count = 0;

    auto cleanup = [&]()
    {
        if (event_buffer != nullptr)
            gpiod_edge_event_buffer_free(event_buffer);

        if (line_request != nullptr)
            gpiod_line_request_release(line_request);

        if (request_config != nullptr)
            gpiod_request_config_free(request_config);

        if (line_config != nullptr)
            gpiod_line_config_free(line_config);

        if (settings != nullptr)
            gpiod_line_settings_free(settings);

        if (chip != nullptr)
            gpiod_chip_close(chip);

        if (spi_fd >= 0)
            close(spi_fd);
    };

    /*
     * Open SPI device.
     */
    spi_fd = open(SPI_DEVICE, O_RDWR);

    if (spi_fd < 0)
    {
        std::cerr << "[ERROR] Cannot open "
                  << SPI_DEVICE << ": "
                  << std::strerror(errno) << '\n';
        cleanup();
        return 1;
    }

    uint8_t mode = SPI_MODE_VALUE;
    uint8_t bits = SPI_BITS;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
        std::cerr << "[ERROR] Cannot configure SPI: "
                  << std::strerror(errno) << '\n';
        cleanup();
        return 1;
    }

    /*
     * Configure GPIO25 rising-edge event.
     */
    chip = gpiod_chip_open(GPIO_CHIP);

    if (chip == nullptr)
    {
        std::cerr << "[ERROR] Cannot open GPIO chip: "
                  << std::strerror(errno) << '\n';
        cleanup();
        return 1;
    }

    settings = gpiod_line_settings_new();

    if (settings == nullptr)
    {
        std::cerr << "[ERROR] Cannot allocate GPIO settings\n";
        cleanup();
        return 1;
    }

    if (gpiod_line_settings_set_direction(
            settings, GPIOD_LINE_DIRECTION_INPUT) < 0 ||
        gpiod_line_settings_set_edge_detection(
            settings, GPIOD_LINE_EDGE_RISING) < 0)
    {
        std::cerr << "[ERROR] Cannot configure GPIO25: "
                  << std::strerror(errno) << '\n';
        cleanup();
        return 1;
    }

    line_config = gpiod_line_config_new();

    if (line_config == nullptr)
    {
        std::cerr << "[ERROR] Cannot allocate GPIO line config\n";
        cleanup();
        return 1;
    }

    const unsigned int offsets[] = { GPIO_LINE };

    if (gpiod_line_config_add_line_settings(
            line_config, offsets, 1, settings) < 0)
    {
        std::cerr << "[ERROR] Cannot add GPIO25 settings: "
                  << std::strerror(errno) << '\n';
        cleanup();
        return 1;
    }

    request_config = gpiod_request_config_new();

    if (request_config == nullptr)
    {
        std::cerr << "[ERROR] Cannot allocate GPIO request config\n";
        cleanup();
        return 1;
    }

    gpiod_request_config_set_consumer(
        request_config, "spi_stm32_rx_test");

    line_request = gpiod_chip_request_lines(
        chip, request_config, line_config);

    if (line_request == nullptr)
    {
        std::cerr << "[ERROR] Cannot request GPIO25: "
                  << std::strerror(errno) << '\n';
        cleanup();
        return 1;
    }

    const int gpio_fd =
        gpiod_line_request_get_fd(line_request);

    if (gpio_fd < 0)
    {
        std::cerr << "[ERROR] Cannot get GPIO event fd\n";
        cleanup();
        return 1;
    }

    event_buffer = gpiod_edge_event_buffer_new(1);

    if (event_buffer == nullptr)
    {
        std::cerr << "[ERROR] Cannot allocate event buffer\n";
        cleanup();
        return 1;
    }

    std::cout
        << "========================================\n"
        << " STM32H755 SPI RX Diagnostic Test\n"
        << "========================================\n"
        << "SPI device : " << SPI_DEVICE << '\n'
        << "SPI mode   : 0\n"
        << "SPI bits   : 8\n"
        << "SPI speed  : " << SPI_SPEED_HZ << " Hz\n"
        << "SPI length : " << SPI_DATA_SIZE << " bytes\n"
        << "GPIO       : " << GPIO_LINE << " rising edge\n"
        << "Trigger    : GPIO25 only\n"
        << "Exit       : Ctrl+C\n"
        << "========================================\n"
        << "Waiting for GPIO25 rising edges...\n";

    std::cout.flush();

    while (!stop_requested)
    {
        struct pollfd pfd{};
        pfd.fd = gpio_fd;
        pfd.events = POLLIN;

        const int poll_result = poll(&pfd, 1, -1);

        if (poll_result < 0)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "[GPIO] poll failed: "
                      << std::strerror(errno) << '\n';
            break;
        }

        if (!(pfd.revents & POLLIN))
        {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                std::cerr << "[GPIO] GPIO event descriptor error\n";
                break;
            }

            continue;
        }

        const int event_count =
            gpiod_line_request_read_edge_events(
                line_request, event_buffer, 1);

        if (event_count < 0)
        {
            std::cerr << "[GPIO] Read event failed: "
                      << std::strerror(errno) << '\n';
            continue;
        }

        if (event_count == 0)
            continue;

        ++edge_count;

        uint8_t tx[SPI_DATA_SIZE]{};
        uint8_t rx[SPI_DATA_SIZE]{};

        struct spi_ioc_transfer transfer{};

        transfer.tx_buf =
            reinterpret_cast<unsigned long>(tx);
        transfer.rx_buf =
            reinterpret_cast<unsigned long>(rx);
        transfer.len = static_cast<uint32_t>(SPI_DATA_SIZE);
        transfer.speed_hz = SPI_SPEED_HZ;
        transfer.bits_per_word = SPI_BITS;
        transfer.delay_usecs = 0;

        const int result =
            ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);

        if (result != static_cast<int>(SPI_DATA_SIZE))
        {
            ++transfer_error_count;

            std::cerr
                << "[SPI ERROR] GPIO edge=" << edge_count
                << " result=" << result
                << " errno=" << std::strerror(errno)
                << '\n';
            continue;
        }

        ++transfer_count;

        size_t zero_count = 0;
        size_t ff_count = 0;

        for (uint8_t byte : rx)
        {
            if (byte == 0x00)
                ++zero_count;

            if (byte == 0xFF)
                ++ff_count;
        }

        std::cout
            << "[RX] edge=" << edge_count
            << " transfer=" << transfer_count
            << " bytes=" << SPI_DATA_SIZE
            << " zero=" << zero_count
            << " ff=" << ff_count
            << " first16=";

        for (size_t i = 0; i < 16; ++i)
        {
            std::cout
                << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0')
                << static_cast<unsigned int>(rx[i]);

            if (i != 15)
                std::cout << ' ';
        }

        std::cout << std::dec << std::setfill(' ') << '\n';
        std::cout.flush();
    }

    std::cout
        << "\n========== Summary ==========\n"
        << "GPIO edges       : " << edge_count << '\n'
        << "SPI transfers OK : " << transfer_count << '\n'
        << "SPI errors       : " << transfer_error_count << '\n'
        << "=============================\n";

    cleanup();
    return 0;
}
