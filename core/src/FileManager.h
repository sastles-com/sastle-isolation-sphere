#ifndef __FILE_MANAGER_H__
#define __FILE_MANAGER_H__

#include "common.h"
#include <Arduino.h>

#ifdef ESP32
#include <LittleFS.h>
#include <FS.h>
#endif

namespace sastle {
    class FileManager {
    public:
        FileManager() {}
        virtual ~FileManager() {}

        // Initialize LittleFS
        static bool begin();
        
        // Unmount LittleFS
        static void end();

        // Read file content into String
        static bool readFile(const char* path, String &output);
        
        // Write String content to file
        static bool writeFile(const char* path, const String &content);
        
        // List directory contents (like ls command)
        static void ls(const char* dirname = "/", uint8_t levels = 0);
        
        // Check if file/directory exists
        static bool exists(const char* path);
        
        // Remove file
        static bool remove(const char* path);
        
        // Get total filesystem size
        static size_t totalBytes();
        
        // Get used filesystem size
        static size_t usedBytes();
        
        // Print filesystem info
        static void printInfo();
    };
}

#endif // __FILE_MANAGER_H__

