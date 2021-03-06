#include "utils.hpp"
#include "current_cfw.hpp"
#include <switch.h>
#include "download.hpp"
#include "extract.hpp"
#include "progress_event.hpp"
#include "json.hpp"
#include "main_frame.hpp"
#include <filesystem>
#include <fstream>

namespace i18n = brls::i18n;
using namespace i18n::literals;

std::vector<std::string> htmlProcess(std::string s, std::regex rgx){
    //std::vector<std::regex> rgx = {std::regex(">(?:(?!>).)*?</a>"), std::regex(">(?:(?!>).)*?</a>")};
    std::vector<std::string> res;
    std::smatch match;
    while (std::regex_search(s, match, rgx)){
        res.push_back(match[0].str());
        s = match.suffix();
    }
    return res;
}

void createTree(std::string path){
    std::string delimiter = "/";
    size_t pos = 0;
    std::string token;
    std::string directories("");
    while ((pos = path.find(delimiter)) != std::string::npos) {
        token = path.substr(0, pos);
        directories += token + "/";
        std::filesystem::create_directory(directories);
        path.erase(0, pos + delimiter.length());
    }
}

bool isArchive(const char * path){
    std::fstream file;
    std::string fileContent;
    if(std::filesystem::exists(path)){
        file.open(path, std::fstream::in);
        file >> fileContent;
        file.close();
    }
    return fileContent.find("DOCTYPE") == std::string::npos;
}

void downloadArchive(std::string url, archiveType type){
    createTree(DOWNLOAD_PATH);
    AppletType at;
    switch(type){
        case sigpatches:
            downloadFile(url.c_str(), SIGPATCHES_FILENAME, OFF);
            break;
        case cheats:
            downloadFile(url.c_str(), CHEATS_FILENAME, OFF);
            break;
        case fw:
            at = appletGetAppletType();
            if (at == AppletType_Application || at == AppletType_SystemApplication) {
                downloadFile(url.c_str(), FIRMWARE_FILENAME, OFF);
            }
            else{
                brls::Application::crash("menus/utils/fw_warning"_i18n);
            }
            break;
        case app:
            downloadFile(url.c_str(), APP_FILENAME, OFF);
            break;
        case cfw:
            downloadFile(url.c_str(), CFW_FILENAME, OFF);
            break;
        case ams_cfw:
            downloadFile(url.c_str(), AMS_FILENAME, OFF);
    }
}


int showDialogBox(std::string text, std::string opt){
    int dialogResult = -1;
    int result = -1;
    brls::Dialog* dialog = new brls::Dialog(text);
    brls::GenericEvent::Callback callback = [dialog, &dialogResult](brls::View* view) {
        dialogResult = 0;
        dialog->close();
    };
    dialog->addButton(opt, callback);
    dialog->setCancelable(false);
    dialog->open();
    while(result == -1){
        usleep(1);
        result = dialogResult;
    }
    return result;
}

int showDialogBox(std::string text, std::string opt1, std::string opt2){
    int dialogResult = -1;
    int result = -1;
    brls::Dialog* dialog = new brls::Dialog(text);
    brls::GenericEvent::Callback callback1 = [dialog, &dialogResult](brls::View* view) {
        dialogResult = 0;
        dialog->close();
    };
    brls::GenericEvent::Callback callback2 = [dialog, &dialogResult](brls::View* view) {
        dialogResult = 1;
        dialog->close();
    };
    dialog->addButton(opt1, callback1);
    dialog->addButton(opt2, callback2);
    dialog->setCancelable(false);
    dialog->open();
    while(result == -1){
        usleep(1);
        result = dialogResult;
    }
    return result;
}

void extractArchive(archiveType type, std::string tag){
    int overwriteInis = 0;
    std::vector<std::string> titles;
    std::string nroPath ="sdmc:" + std::string(APP_PATH);
    chdir(ROOT_PATH);
    switch(type){
        case sigpatches:
            if(isArchive(SIGPATCHES_FILENAME)) {
                /* std::string backup(HEKATE_IPL_PATH);
                backup += ".old"; */
                if(std::filesystem::exists(HEKATE_IPL_PATH)){
                    overwriteInis = showDialogBox("menus/utils/overwrite"_i18n + std::string(HEKATE_IPL_PATH) +"?", "menus/common/no"_i18n, "menus/common/yes"_i18n);
                    if(overwriteInis == 0){
                        extract(SIGPATCHES_FILENAME, ROOT_PATH, HEKATE_IPL_PATH);
                    }
                    else{
                        extract(SIGPATCHES_FILENAME);
                    }
                }
                else{
                    extract(SIGPATCHES_FILENAME);
                }
            }
            else{
                brls::Application::crash("menus/utils/wrong_type_sigpatches"_i18n);
            }
            break;
        case cheats: 
            titles = getInstalledTitlesNs();
            titles = excludeTitles(CHEATS_EXCLUDE, titles);
            extractCheats(CHEATS_FILENAME, titles, running_cfw);
            break;
        case fw:
            if(std::filesystem::file_size(FIRMWARE_FILENAME) < 200000){
                brls::Application::crash("menus/utils/wrong_type_sigpatches_downloaded"_i18n);
            }
            else{
                if (std::filesystem::exists(FIRMWARE_PATH)) std::filesystem::remove_all(FIRMWARE_PATH);
                createTree(FIRMWARE_PATH);
                extract(FIRMWARE_FILENAME, FIRMWARE_PATH);
            }
            break;
        case app:
            extract(APP_FILENAME, CONFIG_PATH);
            cp(ROMFS_FORWARDER, FORWARDER_PATH);
            envSetNextLoad(FORWARDER_PATH, ("\"" + std::string(FORWARDER_PATH) + "\"").c_str());
            romfsExit();
            brls::Application::quit();
            break;
        case cfw:
            if(isArchive(CFW_FILENAME)){
                overwriteInis = showDialogBox("menus/utils/overwrite_inis"_i18n, "menus/common/no"_i18n, "menus/common/yes"_i18n);
                extract(CFW_FILENAME, ROOT_PATH, overwriteInis);
            }
            else{
                brls::Application::crash("menus/utils/wrong_type_cfw"_i18n);
            }
            break;
        case ams_cfw:
            if(isArchive(AMS_FILENAME)){
                overwriteInis = showDialogBox("menus/utils/overwrite_inis"_i18n, "menus/common/no"_i18n, "menus/common/yes"_i18n);
                usleep(800000);
                int deleteContents = showDialogBox("menus/ams_update/delete_sysmodules_flags"_i18n, "menus/common/no"_i18n, "menus/common/yes"_i18n);
                if(deleteContents == 1)
                    removeSysmodulesFlags(AMS_CONTENTS);
                extract(AMS_FILENAME, ROOT_PATH, overwriteInis);
            }
            break;
    }
    if(std::filesystem::exists(COPY_FILES_JSON))
        copyFiles(COPY_FILES_JSON);
}

void progressTest(std::string url, archiveType type){
    ProgressEvent::instance().reset();
    ProgressEvent::instance().setTotalSteps(2);
    ProgressEvent::instance().setStep(1);
    ProgressEvent::instance().setStep(2);
}

std::string formatListItemTitle(const std::string str, size_t maxScore) {
    size_t score = 0;
    for (size_t i = 0; i < str.length(); i++)
    {
        score += std::isupper(str[i]) ? 4 : 3;
        if(score > maxScore)
        {
            return str.substr(0, i-1) + "\u2026";
        }
    }
    return str;
}

std::string formatApplicationId(u64 ApplicationId){
    std::stringstream strm;
    strm << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << ApplicationId;
    return strm.str();
}

std::set<std::string> readLineByLine(const char * path){
    std::set<std::string> titles;
    std::string str;
    std::ifstream in(path);
    if(in){
        while (std::getline(in, str))
        {
            if(str.size() > 0)
                titles.insert(str);
        }
        in.close();
    }
    return titles;
}

std::vector<std::string> fetchPayloads(){
    std::vector<std::string> payloadPaths;
    payloadPaths.push_back(ROOT_PATH);
    if(std::filesystem::exists(PAYLOAD_PATH))       payloadPaths.push_back(PAYLOAD_PATH);
    if(std::filesystem::exists(AMS_PATH))           payloadPaths.push_back(AMS_PATH);
    if(std::filesystem::exists(REINX_PATH))         payloadPaths.push_back(REINX_PATH);
    if(std::filesystem::exists(BOOTLOADER_PATH))    payloadPaths.push_back(BOOTLOADER_PATH);
    if(std::filesystem::exists(BOOTLOADER_PL_PATH)) payloadPaths.push_back(BOOTLOADER_PL_PATH);
    if(std::filesystem::exists(SXOS_PATH))          payloadPaths.push_back(SXOS_PATH);
    std::vector<std::string> res;
    for (auto& path : payloadPaths){
        for (const auto & entry : std::filesystem::directory_iterator(path)){
            if(entry.path().extension().string() == ".bin"){
                if(entry.path().string() != FUSEE_SECONDARY && entry.path().string() != FUSEE_MTC)
                    res.push_back(entry.path().string().c_str());
            }
        }

    }
    return res;
}

void shut_down(bool reboot){
    bpcInitialize();
    if(reboot) bpcRebootSystem();
    else bpcShutdownSystem();
    bpcExit();
}

std::string getLatestTag(const char *url){
    nlohmann::json tag = getRequest(url, {"accept: application/vnd.github.v3+json"});
    if(tag.find("tag_name") != tag.end())
        return tag["tag_name"];
    else
        return "";
}

void cp(const char *from, const char *to){
    std::ifstream src(from, std::ios::binary);
    std::ofstream dst(to, std::ios::binary);

    if (src.good() && dst.good()) {
        dst << src.rdbuf();
    }
}

Result CopyFile(const char src_path[FS_MAX_PATH], const char dest_path[FS_MAX_PATH]) {
    FsFileSystem *fs;
    Result ret = 0;
    FsFile src_handle, dest_handle;
    int PREVIOUS_BROWSE_STATE = 0;
    FsFileSystem devices[4];
    devices[0] = *fsdevGetDeviceFileSystem("sdmc");
    fs = &devices[0];

    ret = fsFsOpenFile(&devices[PREVIOUS_BROWSE_STATE], src_path, FsOpenMode_Read, &src_handle);
    if (R_FAILED(ret)) {
        return ret;
    }
    
    s64 size = 0;
    ret = fsFileGetSize(&src_handle, &size);
    if (R_FAILED(ret)){
        fsFileClose(&src_handle);
        return ret;
    }
    
    std::filesystem::remove(dest_path);
    fsFsCreateFile(fs, dest_path, size, 0);
        
    ret = fsFsOpenFile(fs, dest_path, FsOpenMode_Write, &dest_handle);
    if (R_FAILED(ret)){
        fsFileClose(&src_handle);
        return ret;
    }
    
    uint64_t bytes_read = 0;
    const u64 buf_size = 0x10000;
    s64 offset = 0;
    unsigned char *buf = new unsigned char[buf_size];
    std::string filename = std::filesystem::path(src_path).filename();
    
    do {
        std::memset(buf, 0, buf_size);
        
        ret = fsFileRead(&src_handle, offset, buf, buf_size, FsReadOption_None, &bytes_read);
        if (R_FAILED(ret)) {
            delete[] buf;
            fsFileClose(&src_handle);
            fsFileClose(&dest_handle);
            return ret;
        }
        ret = fsFileWrite(&dest_handle, offset, buf, bytes_read, FsWriteOption_Flush);
        if (R_FAILED(ret)) {
            delete[] buf;
            fsFileClose(&src_handle);
            fsFileClose(&dest_handle);
            return ret;
        }
        
        offset += bytes_read;
    } while(offset < size);
    
    delete[] buf;
    fsFileClose(&src_handle);
    fsFileClose(&dest_handle);
    return 0;
}

void saveVersion(std::string version, const char* path){
    std::fstream newVersion;
    newVersion.open(path, std::fstream::out | std::fstream::trunc);
    newVersion << version << std::endl;
    newVersion.close();
}

std::string readVersion(const char* path){
    std::fstream versionFile;
    std::string version = "0";
    if(std::filesystem::exists(path)){
        versionFile.open(path, std::fstream::in);
        versionFile >> version;
        versionFile.close();
    }
    return version;
}

std::string copyFiles(const char* path) {
    nlohmann::ordered_json toMove;
    std::ifstream f(COPY_FILES_JSON);
    f >> toMove;
    f.close();
    std::string error = "";
    for (auto it = toMove.begin(); it != toMove.end(); ++it) {
        if(std::filesystem::exists(it.key())) {
            createTree(std::string(std::filesystem::path(it.value().get<std::string>()).parent_path()) + "/");
            cp(it.key().c_str(), it.value().get<std::string>().c_str());
            //std::cout << it.key() << it.value().get<std::string>() << std::endl;
        }
        else {
            error += it.key() + "\n";
        }
    }
    if(error == "") {
        error = "menus/common/all_done"_i18n;
    }
    else {
        error = "menus/tools/batch_copy_not_found"_i18n + error;
    }
    return error;
}

int removeDir(const char* path) {
    Result ret = 0;
    FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");
    if (R_FAILED(ret = fsFsDeleteDirectoryRecursively(fs, path))) {
        return ret;
    }
    return 0;
}

bool isErista() {
    splInitialize();
    u64 hwType;
    Result rc = splGetConfig(SplConfigItem_HardwareType, &hwType);
    splExit();

    if(R_FAILED(rc))
        return true;

    switch (hwType)
    {
        case 0:
        case 1:
            return true;
        case 2:
        case 3:
        case 4:
        case 5:
            return false;
        default:
            return true;
    }
};

void removeSysmodulesFlags(const char * directory) {
    const std::set<std::string> AMS_SYSMODULES{
        "010000000000000D",
        "010000000000002B",
        "0100000000000032",
        "0100000000000036",
        "0100000000000042",
        "0100000000000008",
        "010000000000003C",
        "0100000000000034",
        "0100000000000037"
    };
    bool found = false;
    for (const auto & e : std::filesystem::recursive_directory_iterator(directory)) {
        if(e.path().string().find("boot2.flag") != std::string::npos) {
            for(const auto & c : AMS_SYSMODULES) {
                if(e.path().string().find(c) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if(!found)
                std::filesystem::remove(e.path());
        }
        found = false;
    }
}