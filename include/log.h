#ifndef LOG_H_
#define LOG_H_
#include <fstream>
#include <iostream>
std::ofstream logFileObj("../log/logger.txt");

Logger* Logger::instance = nullptr;


class Logger {
private:
    static Logger* instance;
    Logger(){
        if (logger == nullptr) {
            logger = 
        }
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger() {
        logFileObj.close(); //关闭日志文件
    }
public:
    static Logger* getInstance(){
        if (instance == nullptr) {
            instance = new Logger();
        }
        return instance;
    }
    void writeLogger(Level level, const std::string& msg){
        switch (level) {
            case INFO:
                logFileObj << "INFO: " << msg << std::endl;
                break;
            case WARNING:
                logFileObj << "WARNING: " << msg << std::endl;
                break;
            case ERROR:
                logFileObj << "ERROR: " << msg << std::endl;
                break;
            default:
                logFileObj << "INFO: " << msg << std::endl;
                break;
        }
    }
public:
    tydef enum {
        INFO,
        WARNING,
        ERROR
    } Level;
}