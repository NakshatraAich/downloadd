#include <iomanip>
#include <iostream>
#include <cstring>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <unordered_map>

struct FileEvent {
    std::string timestamp;
    std::filesystem::path path;
    uint32_t mask;
    bool isDirectory;
};

static std::string logMethod(const FileEvent& fileEvent, const std::string& message) {
    std::stringstream ss;
    ss << fileEvent.timestamp << " | " << (fileEvent.isDirectory ? "[DIR] " : "[FILE] ") << message << " | Path: " << fileEvent.path.string() << std::endl; 
    
    return ss.str();
}

static std::string returnCurrentTime() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static int initializeInotify(){
    int fd = inotify_init1(0);

    if (fd == -1) {
        std::cerr << "Error adding watch: " << strerror(errno) << std::endl;
        return 1;
    }

    return fd;
}

static void addWatchRecursive(int fd, const std::filesystem::path& path, uint32_t mask, std::unordered_map<int, std::filesystem::path> &watchMap) {
    int watch = inotify_add_watch(fd, path.c_str(), mask);
    if (watch == -1) {
        std::cerr << "Error adding watch: " << strerror(errno) << std::endl;
    }
    watchMap[watch] = path;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory()){
            addWatchRecursive(fd, entry.path(), mask, watchMap);
        }
            
    }
}

static std::unordered_map<int, std::filesystem::path> addWatchHelper(int fd, const std::filesystem::path& path, uint32_t mask) {    
    std::unordered_map<int, std::filesystem::path> watchMap;
    
    int watch = inotify_add_watch(fd, path.c_str(), mask);
    if (watch == -1) {
        std::cerr << "Error adding watch: " << strerror(errno) << std::endl;
        return {};
    }
    watchMap[watch] = path;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory()){
            addWatchRecursive(fd, entry.path(), mask, watchMap);
        }
            
    }

    return watchMap;
}

int main() {

    std::filesystem::path pathToWatch = "/home/nakshatra/dev/download-daemon/test";
    std::ofstream logFile("download-daemon.log", std::ios::app);

    int in = initializeInotify();
    std::unordered_map<int, std::filesystem::path> watchMap = addWatchHelper(in, pathToWatch, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);

    char buffer[4096];

    while (true) {
        ssize_t bytesRead = read(in, buffer, sizeof(buffer));

        if (bytesRead == -1) {
            std::cerr << strerror(errno) << '\n';
            continue;
        }  
        
        size_t offset = 0;

        while (offset < bytesRead) {
            
            bool renameFlag = false;
            inotify_event* event = reinterpret_cast<inotify_event*>(buffer + offset);

            // FileEvent structure to hold the event details
            FileEvent fileEvent;
            fileEvent.timestamp = returnCurrentTime();
            fileEvent.mask = event->mask;
            fileEvent.path = pathToWatch / watchMap[event -> wd] / event->name;

            // Check if the event is for a directory
            if (fileEvent.mask & IN_ISDIR)
                fileEvent.isDirectory = true;
            else
                fileEvent.isDirectory = false;

            // Check the type of event (CREATE | DELETE | MODIFY)
            if (fileEvent.mask & IN_CREATE){
                std::cout << "CREATE: ";
                logFile <<logMethod(fileEvent, "CREATE");
                if (fileEvent.isDirectory){
                    addWatchRecursive(in, fileEvent.path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO, watchMap);
                }
            }

            if (fileEvent.mask & IN_DELETE){
                std::cout << "DELETE: ";
                logFile << logMethod(fileEvent, "DELETE");
                if (fileEvent.isDirectory){
                    int watchToRemove = -1;
                    for (const auto& pair : watchMap) {
                        if (pair.second == fileEvent.path) {
                            watchToRemove = pair.first;
                            break;
                        }
                    }
                    if (watchToRemove != -1) {
                        inotify_rm_watch(in, watchToRemove);
                        watchMap.erase(watchToRemove);
                    }
                }
            }

            if (fileEvent.mask & IN_MODIFY){
                std::cout << "MODIFY: ";
                logFile << logMethod(fileEvent, "MODIFY");
            }
            
            // Check for RENAME event (or MOVED_FROM | MOVED_TO)
            if (fileEvent.mask & IN_MOVED_FROM){
                
                offset += sizeof(inotify_event) + event->len;
                if (offset < bytesRead) {
                    inotify_event* nextEvent = reinterpret_cast<inotify_event*>(buffer + offset);

                    if(nextEvent -> mask & IN_MOVED_TO && nextEvent -> cookie == event -> cookie){
                        std::cout << "RENAMED FROM: " << fileEvent.path.filename().string() << " -> " << nextEvent->name << std::endl;
                        logFile << logMethod(fileEvent, "RENAMED FROM " +fileEvent.path.filename().string() + " -> " + nextEvent->name);
                        renameFlag = true;
                    }else{
                        std::cout << "MOVED FROM: ";
                        logFile <<logMethod(fileEvent, "MOVED FROM");
                        offset -= sizeof(inotify_event) + event->len;
                    }
                }else{
                    std::cout << "MOVED FROM: ";
                    logFile <<logMethod(fileEvent, "MOVED FROM");
                    offset -= sizeof(inotify_event) + event->len;
                }
            }
            
            if (fileEvent.mask & IN_MOVED_TO){
                logFile << logMethod(fileEvent, "MOVED TO");
                std::cout << "MOVED TO ";
            }

            if (fileEvent.path.has_filename() && !renameFlag)
                std::cout << fileEvent.path.filename().string() << " (Cookie: " << fileEvent.mask << ")" << fileEvent.path / fileEvent.path.filename() << std::endl;

            offset += sizeof(inotify_event) + event->len;
            logFile.flush();
        }
    }
}
