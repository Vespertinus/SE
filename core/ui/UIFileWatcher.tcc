#ifdef SE_UI_HOT_RELOAD

#include <sys/inotify.h>
#include <unistd.h>
#include <cstring>
#include <Logging.h>

namespace SE {

UIFileWatcher::UIFileWatcher() {

        fd = inotify_init1(IN_NONBLOCK);
        if (fd < 0)
                log_w("UIFileWatcher: inotify_init1 failed (errno {})", errno);
}

UIFileWatcher::~UIFileWatcher() {

        if (fd >= 0) {
                close(fd);
                fd = -1;
        }
}

void UIFileWatcher::WatchDir(const std::string & dir_path) {

        if (fd < 0) return;

        // Avoid duplicate watches for the same path
        for (auto & [wd, path] : mWDs)
                if (path == dir_path) return;

        const int wd = inotify_add_watch(fd, dir_path.c_str(),
                                         IN_MODIFY | IN_CREATE | IN_MOVED_TO);
        if (wd < 0) {
                log_w("UIFileWatcher: inotify_add_watch('{}') failed (errno {})",
                      dir_path, errno);
                return;
        }
        mWDs[wd] = dir_path;
        log_i("UIFileWatcher: watching '{}'", dir_path);
}

std::vector<std::string> UIFileWatcher::Poll() {

        std::vector<std::string> changed;
        if (fd < 0) return changed;

        // Buffer sized for ~16 events; inotify_event is variable-length
        constexpr size_t kBufSize = sizeof(inotify_event) * 16 + NAME_MAX + 1;
        alignas(inotify_event) char buf[kBufSize];

        ssize_t n;
        while ((n = read(fd, buf, kBufSize)) > 0) {
                const char * ptr = buf;
                while (ptr < buf + n) {
                        const auto * ev = reinterpret_cast<const inotify_event *>(ptr);
                        ptr += sizeof(inotify_event) + ev->len;

                        if (!(ev->mask & (IN_MODIFY | IN_CREATE | IN_MOVED_TO))) continue;
                        if (!ev->len || ev->name[0] == '\0') continue;

                        const std::string name(ev->name);
                        const bool is_rml  = name.size() >= 4 &&
                                             name.compare(name.size() - 4, 4, ".rml")  == 0;
                        const bool is_rcss = name.size() >= 5 &&
                                             name.compare(name.size() - 5, 5, ".rcss") == 0;
                        if (!is_rml && !is_rcss) continue;

                        auto it = mWDs.find(ev->wd);
                        if (it == mWDs.end()) continue;

                        changed.push_back(it->second + "/" + name);
                }
        }
        return changed;
}

} // namespace SE

#endif // SE_UI_HOT_RELOAD
