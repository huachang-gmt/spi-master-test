#include <gpiod.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{

constexpr const char* GPIO_CHIP = "/dev/gpiochip0";
constexpr unsigned int GPIO25 = 25;

const char* EdgeTypeToString(
    const gpiod::edge_event& event)
{
    switch (event.type())
    {
        case gpiod::edge_event::event_type::RISING_EDGE:
            return "RISING";

        case gpiod::edge_event::event_type::FALLING_EDGE:
            return "FALLING";

        default:
            return "UNKNOWN";
    }
}

} // namespace

int main()
{
    std::cout << "========================================\n";
    std::cout << "CM5 GPIO25 Interrupt Test\n";
    std::cout << "========================================\n";
    std::cout << "GPIO chip : " << GPIO_CHIP << "\n";
    std::cout << "GPIO line : " << GPIO25 << "\n";
    std::cout << "Edge      : RISING\n";
    std::cout << "========================================\n";
    std::cout << "Waiting for STM32H755 PE3...\n";
    std::cout << std::flush;

    try
    {
        /*
         * GPIO25 is an input.
         *
         * Only rising edge is monitored.
         *
         * STM32:
         *
         *   PE3 LOW
         *       |
         *       |  HIGH
         *       v
         *   CM5 GPIO25 -> RISING EDGE
         */
        auto request =
            gpiod::chip(GPIO_CHIP)
                .prepare_request()
                .set_consumer("cm5-gpio25-test")
                .add_line_settings(
                    GPIO25,
                    gpiod::line_settings()
                        .set_direction(
                            gpiod::line::direction::INPUT)
                        .set_edge_detection(
                            gpiod::line::edge::RISING)
                )
                .do_request();

        /*
         * One event at a time is enough for this
         * first hardware test.
         */
        gpiod::edge_event_buffer buffer(1);

        uint64_t event_count = 0;

        while (true)
        {
            /*
             * Block here until GPIO25 generates
             * a rising-edge event.
             */
            request.read_edge_events(buffer);

            for (const auto& event : buffer)
            {
                ++event_count;

                auto now =
                    std::chrono::system_clock::now();

                auto time =
                    std::chrono::system_clock::to_time_t(now);

                std::cout
                    << "[" << std::put_time(
                           std::localtime(&time),
                           "%H:%M:%S")
                    << "] "
                    << "GPIO25 "
                    << EdgeTypeToString(event)
                    << "  event="
                    << event_count
                    << "  line_seq="
                    << event.line_seqno()
                    << "\n"
                    << std::flush;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "\n[ERROR] "
            << e.what()
            << "\n";

        return 1;
    }

    return 0;
}