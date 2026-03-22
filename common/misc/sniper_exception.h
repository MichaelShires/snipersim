#ifndef SNIPER_EXCEPTION_H
#define SNIPER_EXCEPTION_H

#include <exception>
#include <string>

class SniperException : public std::exception {
private:
    std::string m_message;

public:
    SniperException(const std::string& message) : m_message(message) {}
    virtual const char* what() const noexcept override {
        return m_message.c_str();
    }
};

class ConfigurationException : public SniperException {
public:
    ConfigurationException(const std::string& message) : SniperException("Configuration Error: " + message) {}
};

class SimulationException : public SniperException {
public:
    SimulationException(const std::string& message) : SniperException("Simulation Error: " + message) {}
};

#endif // SNIPER_EXCEPTION_H
