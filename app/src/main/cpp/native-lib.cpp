#include <thread>
#include "dlfcn.h"
#include <filesystem>
#include "fstream"
#include <fcntl.h>
#include <android/log.h>
#include <unistd.h>
#include "jni.h"

using namespace std;

#define _TAG "ModLoader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,_TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,_TAG,__VA_ARGS__)

//just makes it easier
namespace fs = std::__fs::filesystem;

const char *GetPackageName() {
    char *application_id[256] = {0};
    FILE *fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
        fread(application_id, sizeof(application_id), 1, fp);
        fclose(fp);
    }
    return (const char *) application_id;
}

void Load(){

    //get directory that can be easily accessed
    string localDirectory = string("/storage/emulated/0/Android/data/").append(
            GetPackageName());
    //the folder where all mods will be placed
    string localModsDirectory = localDirectory.append("/nativemods");

    //check if the nativemods folder exists
    if(!fs::exists(localModsDirectory)) {
        //if not create the folder
        fs::create_directory(localModsDirectory.c_str());
        LOGI("SETUP FINISHED, PLEASE RESTART GAME");
        //return so the rest of the code does not run
        return;
    }
    //go through every file in the nativemods folder
    for (const auto& entry : fs::directory_iterator(localModsDirectory)) {
        //check if it a normal file
        if (!fs::is_regular_file(entry.status())) {
            continue;
        }
        //make sure it is a native library
        if(entry.path().extension() != ".so"){
            continue;
        }

        //full path of the file in the nativemods folder
        string external_path = entry.path();
        // internal path that can be used to load libraries
        //kept as a string for safety
        std::string internalPath = string("/data/data/") + GetPackageName() + "/files/" + entry.path().filename().string();

        //check if an instance of this file exists in the internal directory and if so, delete it
        if(fs::exists(internalPath)) fs::remove(internalPath);

        //copy the file in the nativemods directory into the internal directory
        //this is because we are unable to load external files
        try {
            if (!fs::copy_file(external_path, internalPath, fs::copy_options::overwrite_existing)) {
                LOGE("Failed to copy .so to internal storage.");
                continue;
            }
        } catch (const fs::filesystem_error& e) {
            LOGE("Exception during copy_file: %s", e.what());
            continue;
        }

        //load the file with dlopen
        auto handle = dlopen(internalPath.c_str(), RTLD_LAZY);

        //log the state
        if (handle == nullptr) {
            LOGE("Failed to load %s: %s", entry.path().c_str(), dlerror());
        }
        else{
            LOGI("Successfully loaded %s", entry.path().filename().c_str());
        }
    }

}
void TryLoadWithJNI(JavaVM* vm){

    //get directory that can be easily accessed
    string localDirectory = string("/storage/emulated/0/Android/data/").append(
            GetPackageName());
    //the folder where all mods will be placed
    string localModsDirectory = localDirectory.append("/nativemods");

    //check if the nativemods folder exists
    if(!fs::exists(localModsDirectory)) {
        //if not create the folder
        fs::create_directory(localModsDirectory.c_str());
        LOGI("SETUP FINISHED, PLEASE RESTART GAME");
        //return so the rest of the code does not run
        return;
    }
    //go through every file in the nativemods folder
    for (const auto& entry : fs::directory_iterator(localModsDirectory)) {
        //check if it a normal file
        if (!fs::is_regular_file(entry.status())) {
            continue;
        }
        //make sure it is a native library
        if(entry.path().extension() != ".so"){
            continue;
        }

        //full path of the file in the nativemods folder
        string external_path = entry.path();
        // internal path that can be used to load libraries
        //kept as a string for safety
        std::string internalPath = string("/data/data/") + GetPackageName() + "/files/" + entry.path().filename().string();

        //check if an instance of this file exists in the internal directory and if so, delete it
        if(fs::exists(internalPath)) fs::remove(internalPath);

        //copy the file in the nativemods directory into the internal directory
        //this is because we are unable to load external files
        try {
            if (!fs::copy_file(external_path, internalPath, fs::copy_options::overwrite_existing)) {
                LOGE("Failed to copy .so to internal storage.");
                continue;
            }
        } catch (const fs::filesystem_error& e) {
            LOGE("Exception during copy_file: %s", e.what());
            continue;
        }

        //load the file with dlopen
        auto handle = dlopen(internalPath.c_str(), RTLD_LAZY);

        //log the state
        if (handle == nullptr) {
            LOGE("Failed to load %s: %s", entry.path().c_str(), dlerror());
        }
        else{
            LOGI("Successfully loaded %s", entry.path().filename().c_str());
            auto jniOnLoad = (jint (*)(JavaVM*, void*))dlsym(handle, "JNI_OnLoad");
            if(jniOnLoad){
                LOGI("jniOnLoad found in %s", entry.path().filename().c_str());
                jniOnLoad(vm, nullptr);
            }
            else{
                LOGW("JNI_OnLoad not found in %s", entry.path().filename().c_str());
            }
        }
    }

}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, [[maybe_unused]] void *reserved) {
    TryLoadWithJNI(vm);

    return JNI_VERSION_1_6;
}


__attribute__ ((constructor))
void lib_main() {
    std::thread([]() {
        //wait until il2cpp is loaded
        void* handle = nullptr;
        do {
            handle = dlopen("libil2cpp.so", RTLD_LAZY);
            sleep(1);
        } while (handle == nullptr);

        //start the mod loading
        //Load();

    }).detach();
}