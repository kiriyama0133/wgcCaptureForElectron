#pragma once
#include<iostream>
#include<fmt/color.h>
#include<Windows.h>
#include<string>

enum class LoggerLevel
{
	info,
	debug,
	warning,
	error,
};
namespace Logger {
	/**
	* @author: kiriyama
	* @brief a logger function that outputs messages to the console with different log levels.
	*  you can use this class to log messages with different levels of severity, such as info, debug, warning, and error. The function takes a LoggerLevel enum value to specify the log level and a string message to be logged. Depending on the log level, the message will be printed to either std::cout or std::cerr with an appropriate prefix indicating the log level.
	*/
	class LoggerClass {
	public:
		LoggerClass() {
			SetConsoleOutputCP(CP_UTF8);
		}
		LoggerClass(const int&& codePage) {
			SetConsoleOutputCP(codePage);
		}
		/**
		* @author: kiriyama
		* @brief a logger function that outputs messages to the console with different log levels.
		*  @attention: this function is not thread-safe, so if you are using it in a multi-threaded environment, you may want to consider adding synchronization mechanisms to ensure that log messages are not interleaved or lost.
		*/
		void Log(LoggerLevel level, const std::string& message) {
			switch (level) {
			case LoggerLevel::info:
				fmt::print(fg(fmt::color::green) | fmt::emphasis::italic, "{}\n", message);
				break;
			case LoggerLevel::debug:
				fmt::print(fg(fmt::color::blue) | fmt::emphasis::italic, "{}\n", message);
				break;
			case LoggerLevel::warning:
				fmt::print(fg(fmt::color::yellow) | fmt::emphasis::italic, "{}\n", message);
				break;
			case LoggerLevel::error:
				fmt::print(fg(fmt::color::red) | fmt::emphasis::italic, "{}\n", message);
				break;
			default:
				std::cerr << "[UNKNOWN] " << message << std::endl;
				break;
			}
		}
	};
}
