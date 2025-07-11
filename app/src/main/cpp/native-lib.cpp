#include <thread>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <android/log.h>
#include <unistd.h>
#include <jni.h>
#include <cstdlib>

#include "json.hpp" // Download from: https://github.com/nlohmann/json
using json = nlohmann::json;
namespace fs = std::__fs::filesystem;

#define _TAG "ModLoader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,_TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,_TAG,__VA_ARGS__)

const char* GetPackageName() {
    static char application_id[256] = {0};
    FILE* fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
        fread(application_id, sizeof(application_id), 1, fp);
        fclose(fp);
    }
    return application_id;
}

void unzipLmod(const std::string& zipPath, const std::string& destDir) {
    std::string command = "unzip -o \"" + zipPath + "\" -d \"" + destDir + "\"";
    int result = system(command.c_str());
    if (result != 0) {
        LOGE("Failed to unzip %s", zipPath.c_str());
    }
}

void TryLoadWithJNI(JavaVM* vm) {
    std::string localDirectory = std::string("/storage/emulated/0/Android/data/") + GetPackageName();

    if (!fs::exists(localDirectory)) {
        LOGW("Waiting for game to create directory...");
        return;
    }

    std::string localModsDirectory = localDirectory + "/nativemods";
    if (!fs::exists(localModsDirectory)) {
        fs::create_directory(localModsDirectory);
        LOGI("SETUP COMPLETE: Add .lmod files to %s", localModsDirectory.c_str());
        return;
    }

    for (const auto& entry : fs::directory_iterator(localModsDirectory)) {
        if (!fs::is_regular_file(entry.status())) continue;
        if (entry.path().extension() != ".lmod") continue;

        std::string modName = entry.path().stem().string();
        std::string extractPath = std::string("/data/data/") + GetPackageName() + "/files/extracted/" + modName;
        fs::create_directories(extractPath);

        unzipLmod(entry.path().string(), extractPath);

        std::string modJsonPath = extractPath + "/mod.json";
        if (!fs::exists(modJsonPath)) {
            LOGE("mod.json not found in %s", modName.c_str());
            continue;
        }

        json modInfo;
        try {
            modInfo = json::parse(std::ifstream(modJsonPath));
        } catch (std::exception& e) {
            LOGE("Failed to parse mod.json in %s: %s", modName.c_str(), e.what());
            continue;
        }

        std::string soName = modInfo["library"];
        std::string soExtractPath = extractPath + "/" + soName;
        std::string internalSoPath = std::string("/data/data/") + GetPackageName() + "/files/" + soName;

        if (!fs::exists(soExtractPath)) {
            LOGE("Library %s not found in mod %s", soExtractPath.c_str(), modName.c_str());
            continue;
        }

        try {
            fs::copy_file(soExtractPath, internalSoPath, fs::copy_options::overwrite_existing);
        } catch (fs::filesystem_error& e) {
            LOGE("Error copying .so: %s", e.what());
            continue;
        }

        if (modInfo.contains("asset")) {
            std::string assetFile = modInfo["asset"];
            std::string srcAssetPath = extractPath + "/" + assetFile;
            std::string dstAssetPath;

            if (modInfo["engine"] == "unity") {
                dstAssetPath = std::string("/data/data/") + GetPackageName() + "/files/unitymods/" + assetFile;
            } else if (modInfo["engine"] == "unreal") {
                dstAssetPath = std::string("/data/data/") + GetPackageName() + "/files/ue4mods/" + assetFile;
            }

            try {
                if (!dstAssetPath.empty() && fs::exists(srcAssetPath)) {
                    fs::create_directories(fs::path(dstAssetPath).parent_path());
                    fs::copy_file(srcAssetPath, dstAssetPath, fs::copy_options::overwrite_existing);
                    LOGI("Copied asset to %s", dstAssetPath.c_str());
                }
            } catch (fs::filesystem_error& e) {
                LOGW("Asset copy failed for %s: %s", assetFile.c_str(), e.what());
            }
        }

        void* handle = dlopen(internalSoPath.c_str(), RTLD_NOW);
        if (!handle) {
            LOGE("Failed to dlopen %s: %s", internalSoPath.c_str(), dlerror());
            continue;
        }

        LOGI("Successfully loaded: %s", soName.c_str());

        auto jniOnLoad = (jint (*)(JavaVM*, void*))dlsym(handle, "JNI_OnLoad");
        if (jniOnLoad) {
            LOGI("JNI_OnLoad found, invoking...");
            jniOnLoad(vm, nullptr);
        } else {
            LOGW("JNI_OnLoad not found in %s", soName.c_str());
        }
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    TryLoadWithJNI(vm);
    return JNI_VERSION_1_6;
}
