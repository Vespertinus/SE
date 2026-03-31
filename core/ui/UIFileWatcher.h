#ifndef __UI_FILE_WATCHER_H__
#define __UI_FILE_WATCHER_H__ 1

#ifdef SE_UI_HOT_RELOAD

#include <string>
#include <unordered_map>
#include <vector>

namespace SE {

// Non-blocking inotify-based file watcher.
// Call WatchDir() for each directory to monitor, then Poll() each frame
// to get the list of changed file paths since the last call.
class UIFileWatcher {

        int                               fd{-1};        // inotify instance fd
        std::unordered_map<int, std::string> mWDs;       // wd → directory path

public:
        UIFileWatcher();
        ~UIFileWatcher();

        // Begin watching a directory (IN_MODIFY | IN_CREATE).
        // Safe to call multiple times for the same directory.
        void WatchDir(const std::string & dir_path);

        // Returns a list of changed file paths (.rml / .rcss) since the last call.
        // Non-blocking — returns empty vector if nothing changed.
        std::vector<std::string> Poll();
};

} // namespace SE

#endif // SE_UI_HOT_RELOAD
#endif // __UI_FILE_WATCHER_H__
