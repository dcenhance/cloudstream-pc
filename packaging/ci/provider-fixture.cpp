// Native replacement for the POSIX-only test provider scripts on Windows CI.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

int main(int argc, char **argv) {
    const char *video = std::getenv("CLOUDSTREAM_EPISODE_VIDEO");
    if (argc > 1 && std::strcmp(argv[1], "sources") == 0 && video) {
        const char *slow = std::getenv("CLOUDSTREAM_EPISODE_SLOW");
        if (argc > 5 && std::strcmp(argv[5], "ep2") == 0 && slow && std::strcmp(slow, "1") == 0)
            std::this_thread::sleep_for(std::chrono::seconds(2));
        std::printf(R"json({"success":true,"links":[{"source":"Primary","quality":1080,"type":"VIDEO","url":"%s"},{"source":"Alternate","quality":720,"type":"VIDEO","url":"%s?alternate"}]})json", video, video);
        return 0;
    }
    if (video) {
        std::puts(R"json({"name":"Series","episodes":[{"season":1,"episode":1,"name":"One","data":"ep1"},{"season":1,"episode":2,"name":"Two","data":"ep2"}]})json");
    } else {
        std::puts(R"json({"name":"Regression series","plot":"Details must cover Search.","episodes":[{"name":"First episode","season":1,"episode":1,"data":"fixture"}]})json");
    }
    return 0;
}
