#pragma once
#include <string>
#include "Arduino.h" // For String, Stream

class File : public Stream {
public:
    String _name;
    bool _isValid;
    String _content;

    File(String name = "", bool isValid = false) : _name(name), _isValid(isValid) {}
    
    operator bool() const { return _isValid; }
    String name() const { return _name; }
    
    int _iterIndex = 0;
    
    // Mocking openNextFile for directory iteration
    File openNextFile() { 
        if (_iterIndex == 0) {
            _iterIndex++;
            return File("config.json", true);
        } else if (_iterIndex == 1) {
            _iterIndex++;
            return File("led_layout.csv", true);
        }
        return File(); 
    }
};

class LittleFSClass {
public:
    bool begin(bool formatOnFail = false, const char * basePath = "/littlefs", uint8_t maxOpenFiles = 10, const char * partitionLabel = "spiffs") {
        return true;
    }

    bool exists(const char* path) { return true; }

    File open(const char* path, const char* mode = "r") {
        return File(path, true);
    }
};

extern LittleFSClass LittleFS;
